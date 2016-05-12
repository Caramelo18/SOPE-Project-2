#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#define NAMESIZE 10
#define NUM_ENTRANCES   4

unsigned int durationPeriod;
unsigned int minInterval;

struct carInfo
{
    char direction;
    int number;
    int parkingTime;
};

void *carThread(void *arg)
{
    pthread_t ownThread = pthread_self();
    if(pthread_detach(ownThread) != 0)
    {
        printf("Error making thread nr %d detached\n", (int)ownThread);
        exit(1);
    }
    struct carInfo *car = (struct carInfo*) arg;
    printf("thr: car: %c%d - time: %d\n", car->direction, car->number, car->parkingTime);
    char fifoName[6];
    sprintf(fifoName, "fifo%c", car->direction);
//    printf("fifoname: %s\n", fifoName);
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
        exit(2);
    }

    durationPeriod = strtol(argv[1], NULL, 10);
    minInterval = strtol(argv[2],NULL,10);

    if(durationPeriod <= 0 || minInterval <= 0)
    {
        printf("Illegal arguments. Use <unsigned int> <unsigned int>\n");
        exit(3);
    }

    time_t t;
    srand((unsigned) time(&t));

    struct sigaction action;
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM,&action, NULL) < 0)
    {
        fprintf(stderr,"Unable to install SIGALRM handler\n");
        exit(4);
    }
    alarm(durationPeriod);

    time_t begin, end;
    char *fifoNames[NUM_ENTRANCES] = {"N", "S", "E", "W"};
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
            char *direction = fifoNames[index];
            //++carNumbers[index];
            ++carNumber;
            int parkTime = ((rand() % 10) + 1) * minInterval;
            sleepTime = probabilities[rand() % 10] * minInterval;
        //    printf("car: %s%d - time: %d\n", direction, carNumber, parkTime);
            begin = end;

            pthread_t newThread;
            struct carInfo car;
            car.direction = *direction;
            car.number = carNumber;
            car.parkingTime = parkTime;

            pthread_create(&newThread, NULL, carThread, (void *)&car);
        }
    }


}
