#include "utilities.h"

#define LOG             "gerador.log"

short closingTime = 0;
unsigned int minInterval;
//sem_t *entrances[NUM_ENTRANCES];

void updateLog(struct carInfo *car, char message[], clock_t ticks, int start)
{
     FILE *file = fopen(LOG, "a");
     clock_t current = clock();
     if(start == 1)
        fprintf(file, "%9d;%9d;%8c;%12d;    ?    ; %5s\n", (int)ticks, car->number, car->direction, (int)car->parkingTime, message);
    else
        fprintf(file, "%9d;%9d;%8c;%12d;%9d;%5s\n", (int)current, car->number, car->direction, (int)car->parkingTime,  (int)ticks, message);
    fclose(file);
    return;
}

void *carThread(void *arg)
{
    pthread_t ownThread = pthread_self();
    if(pthread_detach(ownThread) != 0)
    {
        printf("Error making thread nr %d detached\n", (int)ownThread);
        exit(1);
    }

    clock_t createTime, endTime;
    createTime = clock();
    struct carInfo *car = (struct carInfo *) arg;
    //printf("thr: car: %cA%d - time: %d - own fifo: %s\n", car.direction, car.number, (int)car.parkingTime, car.fifoName);

    char fifoName[15], semName[15];
    sprintf(semName, "%s%c", SEM_NAME, car->direction);

    sem_t * sem = sem_open(semName,0, 0600, 0);
    if(sem == SEM_FAILED){
        if(errno == ENOENT)
          return NULL;
    }
    // open the correct FIFO
    sprintf(fifoName, "fifo%c", car->direction);
    int fd;
    //printf("%s\n", fifoName);

    fd = open(fifoName, O_WRONLY);  //see O_NONBLOCK
    if(fd == -1){
        if(errno == ENOENT){
            printf("Pipe isn't created yet\n");
            return NULL;
        }
        /*else if(errno == EINTR){
            printf("generator timeout\n");
            free(car);
            return NULL;
        }*/
        printf("Main fifos error %s %d  %d  %d\n", fifoName, car->number, errno , ENXIO  );
        //close(fd);
        free(car);
        return NULL;
    }

    //printf("%c%-10d - %-10d - own fifo: %s\n", car->direction, car->number, (int)car->parkingTime,car->fifoName);
    if(mkfifo(car->fifoName, 0660) == -1)
        printf("Make fifo error %s     %d   %d \n", car->fifoName, errno, EEXIST);

    // passes the car information to the park

    if(write(fd, car, sizeof(struct carInfo)) == -1)
    {
      printf("write error car%d  %d  %d\n", car->number, errno, EBADF);
      close(fd);
      free(car);
      return NULL;
    }

    close(fd);


    // open and write gerador.log
    //FILE *gerador = fopen(LOG, "a");

    // int ind = 0;
    // if(car->direction == 'N')
    //     ind = 0;
    // else if(car->direction == 'S')
    //     ind = 1;
    // else if(car->direction == 'E')
    //     ind = 2;
    // else if(car->direction == 'W')
    //     ind = 3;
    // if(sem_post(sem) == -1){
    //     if(errno == ENOENT)
    //       return NULL;
    // }
    sem_post(sem);

    if(sem_close(sem) == -1)
    {
      if(errno == ENOENT)
        return NULL;
    }
    //}
    // opens its own FIFO
    int carFifo;
    carFifo = open(car->fifoName, O_RDONLY);
    if(carFifo == -1)
        printf("Open error %d %d\n", errno, EMFILE);

    int in = 0;
    char input[6];
    int i =0;
    while(1)
    {
        if((i=read(carFifo, input, sizeof(input)) )> 0){
          //printf("%s\n" , input);
        }
        else if(i == -1)
          printf("%d\n", errno);
        if(strstr(input, OUT) != NULL && in == 1) // if car exits the park
        {
            endTime = clock();
        //    printf("start: %d - end: %d - diff:%d\n", (int)createTime, (int)endTime, (int)(endTime - createTime));
            updateLog(car, OUT, endTime - createTime, 0);
            break;
        }
        else if(strstr(input, IN) != NULL && in == 0)
        {
            updateLog(car, IN, createTime, 1);
            in = 1;
        }
        else if(strstr(input, FULL) != NULL)
        {
            endTime = clock();
            updateLog(car, FULL, endTime - createTime, 0);
            break;
        }
        else if(strstr(input, CLOSED) != NULL && in == 1)
        {
            endTime = clock();
            updateLog(car, CLOSED, endTime - createTime, 0);
            break;
        }
    }


    close(carFifo);
    unlink(car->fifoName);
    //fclose(gerador);
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
    int durationPeriod;
    durationPeriod = strtol(argv[1], NULL, 10);
    minInterval = strtol(argv[2],NULL,10);

    if(durationPeriod <= 0 || minInterval <= 0)
    {
        printf("Illegal arguments. Use <unsigned int> <unsigned int>\n");
        exit(2);
    }

    // clears gerador.log
    FILE *gerador = fopen("gerador.log", "w");
    fprintf(gerador, "t(ticks) ; id_viat ; destin ; t_estacion ; t_vida  ; observ\n" );
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
    int carNumber = 0;
    int index;
    int sleepTime = probabilities[rand() % 10] * minInterval;

    begin = clock();
    while(closingTime != 1)
    {
        end = clock();
        if((end - begin) >= sleepTime)
        {
            //printf("%d    %d\n", (int)(end-begin), sleepTime);
            struct carInfo *vehicle = malloc(sizeof(struct carInfo));
            index = rand() % 4;
            vehicle->direction = fifoNames[index];
            vehicle->parkingTime = ((rand() % 10) + 1) * minInterval;
            vehicle->number = ++carNumber;
            sprintf(vehicle->fifoName, "fifo%c%d", vehicle->direction, carNumber);
            sleepTime = probabilities[rand() % 10] * minInterval;
            //printf("car: %c%d - time: %d\n", direction, carNumber, parkTime);
            //printf("next %d    duration %d   elapsed %lu\n", sleepTime, (int)vehicle->parkingTime, (long) (end-begin));
            pthread_t newThread;
            pthread_create(&newThread, NULL, carThread, (void *)vehicle);
            begin = end;
        }
    }

    pthread_exit(0);
}
