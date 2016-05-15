#include <stdio.h>
#include <stdlib.h>
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
#define MAXTHREADS 300

unsigned int durationPeriod;
unsigned int minInterval;

char SEM_NAME[] = "/sem";

struct carInfo
{
    char direction;
    int number;
    clock_t parkingTime;
    char fifoName[15];
};

struct carInfo carInfos[MAXTHREADS];

void *carThread(void *arg)
{
    pthread_t ownThread = pthread_self();
    if(pthread_detach(ownThread) != 0)
    {
        printf("Error making thread nr %d detached\n", (int)ownThread);
        exit(1);
    }

    struct carInfo *car = (struct carInfo*) arg;
    //printf("thr: car: %c%d - time: %d - own fifo: %s\n", car->direction, car->number, car->parkingTime, car->fifoName);

    char fifoName[6], semName[6];
    sprintf(semName, "%s%c", SEM_NAME, car->direction);

    // open the correct FIFO
    sprintf(fifoName, "fifo%c", car->direction);
    int fd;
    fd = open(fifoName, O_WRONLY);  //see O_NONBLOCK

    sem_t *sem;
    sem = sem_open(semName,0,0600,0);
    if(sem == SEM_FAILED){
      printf("Error opening semaphore\n");
      exit(4);
    }

    // passes the car information to the park
    write(fd, car, sizeof(struct carInfo));

    // open and write gerador.log
    char line[128];
    FILE *gerador = fopen("gerador.log", "a");
    time_t ticks = clock();
//    sprintf(line, );
    fprintf(gerador, "%-10d;%-10d;%-10c;%-10d;   ?\n", (int)ticks, car->number, car->direction, (int)car->parkingTime);


    // opens its own FIFO
    mkfifo(car->fifoName, 0660);
    int carFifo = open(car->fifoName, O_RDONLY | O_NONBLOCK);
    //int n = 0;
    //char input[10];

    /*while(n == 0)
        n = read(carFifo, input, sizeof(input) * sizeof(char));*/


    sprintf(line, "%-10d;%-10d;%-10c;%-10d;%-10d\n", 5, car->number, car->direction, (int)car->parkingTime, 10);
    printf("%s", line);

    sem_post(sem);
    sem_close(sem);
    close(carFifo);
    unlink(car->fifoName);
    fclose(gerador);
    return NULL;
}

void alarm_handler(int signo)
{
    printf("Time's up\n");
    exit(0);
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
    char fifoNames[NUM_ENTRANCES] = {'N', 'S', 'E', 'W'};
    int probabilities[10] = {0,0,0,0,0,1,1,1,2,2};
    //    int carNumbers[4] = {0, 0, 0, 0};
    int carNumber = 0;
    int index;
    int sleepTime = probabilities[rand() % 10] * minInterval;

    begin = clock();
    while(1)
    {
        end = clock();
        if(end - begin >= sleepTime)
        {
            index = rand() % 4;
            char direction = fifoNames[index];
            //++carNumbers[index];
            ++carNumber;
            int parkTime = ((rand() % 10) + 1) * minInterval;
            char carFifoName[15];
            sprintf(carFifoName, "fifo%c%d", direction, carNumber);

            sleepTime = probabilities[rand() % 10] * minInterval;
            //printf("car: %s%d - time: %d\n", direction, carNumber, parkTime);
            begin = end;


            pthread_t newThread;
            carInfos[carNumber].direction = direction;
            carInfos[carNumber].number = carNumber;
            carInfos[carNumber].parkingTime = parkTime;
            strcpy(carInfos[carNumber].fifoName, carFifoName);

            pthread_create(&newThread, NULL, carThread, (void *)&carInfos[carNumber]);
        }
    }

    return 0;
}
