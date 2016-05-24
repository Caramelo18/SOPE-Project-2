#include "utilities.h"

#define LOG             "gerador.log"

short closingTime = 0;
unsigned int minInterval;
pthread_mutex_t mutexLog = PTHREAD_MUTEX_INITIALIZER;
FILE *file;

void updateLog(struct carInfo *car, char message[], clock_t ticks, int start)
{
    clock_t current = clock();
    pthread_mutex_lock(&mutexLog);
    file = fopen(LOG, "a");
    if(start == 1)
        fprintf(file, "%9d;%9d;%8c;%12d;    ?    ; %5s\n", (int)current, car->number, car->direction, (int)car->parkingTime, message);
    else
        fprintf(file, "%9d;%9d;%8c;%12d;%9d;%5s\n", (int)current, car->number, car->direction, (int)car->parkingTime,  (int)(ticks + car->parkingTime), message);
    fclose(file);
    pthread_mutex_unlock(&mutexLog);
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

    sem_t * sem = sem_open(semName,0, 0600, 1);
    if(sem == SEM_FAILED){
      if(errno != ENOENT)
          printf("ola1 %d\n", errno);
      free(car);
      return NULL;

    }

    // open the correct FIFO
    sprintf(fifoName, "fifo%c", car->direction);
    int fd;

    fd = open(fifoName, O_WRONLY | O_NONBLOCK);  //see O_NONBLOCK
    if(fd == -1){
        sem_close(sem);
        free(car);
        return NULL;
    }


    // passes the car information to the park
    if(mkfifo(car->fifoName, 0660) == -1)
        printf("Make fifo error %s     %d   %d \n", car->fifoName, errno, EEXIST);

    if(write(fd, car, sizeof(struct carInfo)) == -1)
    {
      printf("write error car%d  %d  %d\n", car->number, errno, EBADF);
      close(fd);
      sem_close(sem);
      unlink(car->fifoName);
      free(car);
      return NULL;
    }

    close(fd);

    sem_post(sem);

    if(sem_close(sem) == -1)
    {
      if(errno == ENOENT){
        free(car);
        unlink(car->fifoName);
        return NULL;

      }
    }

    // opens its own FIFO
    int carFifo;
    carFifo = open(car->fifoName, O_RDONLY);
    if(carFifo == -1){
        printf("Open error %d %d\n", errno, EMFILE);
        free(car);
        unlink(car->fifoName);
        return NULL;
    }

    int in = 0;
    char input[8];
    int i =0;
    while(1)
    {
        if((i=read(carFifo, input, sizeof(input)) ) <= 0){
          printf("error reading from carFifo %s\n", car->fifoName);
        }

        if(strstr(input, IN) != NULL && in == 0)
        {
            updateLog(car, IN, createTime, 1);
            in = 1;
        }
        else if(strstr(input, OUT) != NULL) // if car exits the park
        {
            endTime = clock();
            updateLog(car, OUT, endTime - createTime, 0);
            break;
        }
        else if(strstr(input, FULL) != NULL)
        {
            endTime = clock();
            updateLog(car, FULL, endTime - createTime, 0);
            break;
        }
        else if(strstr(input, CLOSED) != NULL)
        {
            endTime = clock();
            updateLog(car, CLOSED, endTime - createTime, 0);
            break;
        }
    }

    close(carFifo);
    unlink(car->fifoName);
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
    file = fopen("gerador.log", "w");
    fprintf(file, "t(ticks) ; id_viat ; destin ; t_estacion ; t_vida  ; observ\n" );
    fclose(file);

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
            struct carInfo *vehicle = malloc(sizeof(struct carInfo));
            index = rand() % 4;
            vehicle->direction = fifoNames[index];
            vehicle->parkingTime = ((rand() % 10) + 1) * minInterval;
            vehicle->number = ++carNumber;
            sprintf(vehicle->fifoName, "fifo%c%d", vehicle->direction, vehicle->number);

            pthread_t newThread;
            pthread_create(&newThread, NULL, carThread, (void *)vehicle);

            begin = end;
            sleepTime = probabilities[rand() % 10] * minInterval;

        }
    }

    pthread_exit(0);
}
