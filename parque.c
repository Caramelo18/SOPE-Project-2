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

char SEM_NAME[] = "/sem";

unsigned int parkingSpaces;
unsigned int maxSpaces;
unsigned int closingTime = 0;
sem_t *semaphores[4];
int semIndex = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define NUM_ENTRANCES   4

struct carInfo
{
    char direction;
    int number;
    int parkingTime;
};

void alarm_handler(int signo)
{
    closingTime = 1;
    int i=0;
    for(;i<NUM_ENTRANCES; i++){
      sem_post(semaphores[i]);
    }
}

void *janitor(void *arg){
  return NULL;
}


void *entrancePoints(void *arg)
{
    char fifoName[8], semName[6] ;
    sprintf(fifoName, "fifo%c", *(char *)arg);
    sprintf(semName, "%s%c", SEM_NAME, *(char *)arg);
    printf("Controller: %s   %s\n", fifoName, semName);
    int fifofd;

    mkfifo(fifoName, 0660);
    fifofd = open(fifoName, O_RDONLY | O_NONBLOCK);
    struct carInfo car;

    pthread_mutex_lock(&mutex);
    int a = semIndex++;
    pthread_mutex_unlock(&mutex);
    semaphores[a] = sem_open(semName,O_CREAT,0600,0);
    if(semaphores[a] == SEM_FAILED){
      printf("Error opening semaphore\n");
      exit(4);
    }


    int n;
    do {
      sem_wait(semaphores[a]);

      n = read(fifofd, &car, sizeof(struct carInfo));
      //printf("%d\n", n);


      if(n == 0) {



      }
      else{

        printf("car: %c%d - time: %d\n", car.direction, car.number, car.parkingTime);
      }


    } while (closingTime != 1);
    close(fifofd);
    sem_close(semaphores[a]);
    unlink(fifoName);
    return NULL;
}

int main(int argc, char* argv[]){

    if(argc != 3) {
        printf("Usage: parque <N_LUGARES> <T_ABERTURA>\n");
        exit(1);
    }
    unsigned int workingTime;
    workingTime = strtol(argv[2],NULL,10);
    maxSpaces = strtol(argv[1], NULL, 10);
    parkingSpaces = 0;

    if(workingTime <= 0 || maxSpaces <= 0) {
        printf("Illegal arguments. Use positive <unsigned int> <unsigned int>\n");
        exit(2);
    }

    struct sigaction action;
    action.sa_handler = alarm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM,&action, NULL) < 0)
    {
        fprintf(stderr,"Unable to install SIGALRM handler\n");
        exit(3);
    }

    alarm(workingTime);
    pthread_t threads[NUM_ENTRANCES];
    char fifoNames[NUM_ENTRANCES]= {'N', 'S', 'E', 'W'};

    int i;
    for (i = 0; i < NUM_ENTRANCES; i++)
        pthread_create(&threads[i], NULL, entrancePoints, &fifoNames[i]);

    for (i = 0; i < NUM_ENTRANCES; i++)
      pthread_join(threads[i], NULL);
    return 0;
}
