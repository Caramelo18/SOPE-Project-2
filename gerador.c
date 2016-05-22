#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#define NAMESIZE 10
#define NUM_ENTRANCES   4
#define MAXTHREADS 3000

#define FULL            "FULL"
#define OUT             "OUT"
#define IN              "IN"

const char SEM_NAME[] = "/sem";
const char fifoNames[NUM_ENTRANCES] = {'N', 'S', 'E', 'W'};

struct carInfo
{
    char direction;
    int number;
    clock_t parkingTime;
    char fifoName[15];
};

short closingTime = 0;
unsigned int durationPeriod;
unsigned int minInterval;
sem_t *entrances[NUM_ENTRANCES];
struct carInfo * vehicle;


struct carInfo carInfos[MAXTHREADS];

void *carThread(void *arg)
{
    pthread_t ownThread = pthread_self();
    if(pthread_detach(ownThread) != 0)
    {
        printf("Error making thread nr %d detached\n", (int)ownThread);
        exit(1);
    }

    struct carInfo *car = (struct carInfo *) arg;

    //printf("thr: car: %cA%d - time: %d - own fifo: %s\n", car.direction, car.number, (int)car.parkingTime, car.fifoName);

    char fifoName[15], semName[15];
    sprintf(semName, "%s%c", SEM_NAME, car->direction);

    // open the correct FIFO
    sprintf(fifoName, "fifo%c", car->direction);
    int fd;
    //printf("%s\n", fifoName);

    fd = open(fifoName, O_WRONLY | O_NONBLOCK);  //see O_NONBLOCK
    if(fd == -1){
      if(errno == ENOENT){
        printf("Pipe isn't created yet\n");
        return NULL;
      }
      printf("Main fifos error %s %d  %d  %d\n", fifoName, car->number, errno , ENXIO  );
      close(fd);
      return NULL;
    }

    //printf("%c%-10d - %-10d - own fifo: %s\n", car->direction, car->number, (int)car->parkingTime,car->fifoName);
    if(mkfifo(car->fifoName, 0660) == -1)
      printf("Make fifo error %s     %d   %d \n", car->fifoName, errno, EEXIST);

    // passes the car information to the park

    if(write(fd, car, sizeof(struct carInfo)) == -1){
      printf("write error car%d  %d  %d\n", car->number, errno, EBADF);
      close(fd);
      return NULL;
    }

    close(fd);


    // open and write gerador.log
    //char line[128];
    FILE *gerador = fopen("gerador.log", "a");
    //time_t ticks = clock();
    //    sprintf(line, );
    //    fprintf(gerador, "%-10d;%-10d;%-10c;%-10d;   ?\n", (int)ticks, car->number, car->direction, (int)car->parkingTime);

    /*sem_t *sem;
    sem = sem_open(semName,0,0600,0);
    if(sem == SEM_FAILED){
        //if(errno != EMFILE){
          printf("Error opening semaphore %s %s  %d  %d   %d\n", semName,fifoName, errno,EMFILE, ENOENT);
          //exit(4);
        //}
    }*/
    //else{
    int ind = 'A';
    if(car->direction == 'N')
      ind = 0;
    else if(car->direction == 'S')
      ind = 1;
    else if(car->direction == 'E')
      ind = 2;
    else if(car->direction == 'W')
      ind = 3;
    sem_post(entrances[ind]);
    //sem_close(sem);
    //}
    // opens its own FIFO
    int carFifo;
  //  do {
    carFifo = open(car->fifoName, O_RDONLY);
    if(carFifo == -1)
      printf("Open error %d %d\n", errno, EMFILE);
  //  } while(carFifo == -1);

    int in = 0;
    char input[10];

    while(1)
    {
        read(carFifo, input, sizeof(input));
        if(strcmp(input, OUT) == 0) // if car exits the park
        {
            time_t ticks = clock();
            printf("car %d out\n", car->number);
            fprintf(gerador, "%-10d;%-10d;%-10c;%-10d;   ?; out\n", (int)ticks, car->number, car->direction, (int)car->parkingTime);
            break;
        }
        else if(strcmp(input, IN) == 0 && in == 0)
        {
            time_t ticks = clock();
            printf("car %d in\n", car->number);
            fprintf(gerador, "%-10d;%-10d;%-10c;%-10d;   ?; in\n", (int)ticks, car->number, car->direction, (int)car->parkingTime);
            in = 1;
        }
        else if(strcmp(input, FULL) == 0)
        {
            time_t ticks = clock();
            printf("car %d full\n", car->number);
            fprintf(gerador, "%-10d;%-10d;%-10c;%-10d;   ?; full\n", (int)ticks, car->number, car->direction, (int)car->parkingTime);
            break;
        }
    }


    close(carFifo);
    unlink(car->fifoName);
    fclose(gerador);
    free(car);
    return NULL;
}

void alarm_handler(int signo)
{
    printf("Time's up\n");
    closingTime = 1;

}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        printf("Usage: %s <T_GERACAO> <U_RELOGIO>\n", argv[0]);
        exit(1);
    }

    durationPeriod = strtol(argv[1], NULL, 10);
    minInterval = strtol(argv[2],NULL,10);

    if(durationPeriod <= 0 || minInterval <= 0)
    {
        printf("Illegal arguments. Use <unsigned int> <unsigned int>\n");
        exit(2);
    }

    // clears gerador.log
    FILE *gerador = fopen("gerador.log", "w");
    fclose(gerador);

    time_t t;
    srand((unsigned) time(&t));

    struct sigaction action;
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM,&action, NULL) < 0)
    {
        fprintf(stderr,"Unable to install SIGALRM handler\n");
        exit(3);
    }
    alarm(durationPeriod);



    time_t begin, end;

    int probabilities[10] = {0,0,0,0,0,1,1,1,2,2};
    //    int carNumbers[4] = {0, 0, 0, 0};
    int carNumber = 0; //carIndex = 0;
    int index;
    int sleepTime = probabilities[rand() % 10] * minInterval;

    int i = 0;

    for (;i < NUM_ENTRANCES;i++) {
      char name[7];
      sprintf(name, "%s%c", SEM_NAME,fifoNames[i]);
      //printf("%s\n", name);
      entrances[i] = sem_open(name,0, 0600, 0);
    }

    begin = clock();
    while(closingTime != 1)
    {
        end = clock();
        if((int)(end - begin) >= (int)sleepTime)
        {
          printf("%d    %d\n", (int)(end-begin), sleepTime);
            vehicle = malloc(sizeof(struct carInfo));
            index = rand() % 4;
            vehicle->direction = fifoNames[index];
            //printf("%c\n", vehicle->direction);
            //++carNumber;
            vehicle->parkingTime = ((rand() % 10) + 1) * minInterval;
            printf("%lu\n", (unsigned long)vehicle->parkingTime/CLOCKS_PER_SEC);
            //char carFifoName[15];
            sprintf(vehicle->fifoName, "fifo%c%d", vehicle->direction, carNumber);

            sleepTime = probabilities[rand() % 10] * minInterval;
            //printf("car: %c%d - time: %d\n", direction, carNumber, parkTime);

          //printf("next %d    duration %d   elapsed %lu\n", sleepTime, (int)vehicle->parkingTime, (long) (end-begin));

            pthread_t newThread;


            /*carInfos[carNumber].direction = direction;
            carInfos[carNumber].number = carNumber;
            carInfos[carNumber].parkingTime = parkTime;
            strcpy(carInfos[carNumber].fifoName, carFifoName);
            carIndex++;
            ++carNumber;
            if(carIndex >= MAXTHREADS)
              carIndex = 0;*/
            //car->direction = direction;
            vehicle->number = ++carNumber;
            //car->parkingTime = parkTime;
            //strcpy(car->fifoName, carFifoName);
            //carInfos[carIndex] =*car;
            //printf("%c%-10d - %-10d - own fifo: %s\n", car->direction, car->number, (int)car->parkingTime,car->fifoName);
            pthread_create(&newThread, NULL, carThread, (void *)vehicle);
            //pthread_create(&newThread, NULL, carThread, (void *)&car);
            begin = end;
        }
    }

    for(i=0; i< NUM_ENTRANCES; i++){
      sem_close(entrances[i]);
    }

    return 0;
}
