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

#define NUM_ENTRANCES   4
#define FIFO_SIZE       512*8
#define FULL            "FULL"
#define OUT             "OUT"
#define IN              "IN"



char SEM_NAME[] = "/sem";
const char PRIVATE_FIFO[] = "./temp/fifo";
char FIFO_BACKUP[FIFO_SIZE];
int maxSpaces;

unsigned int parkingSpaces;
unsigned int closingTime = 0;
sem_t *semaphores[4];
int semIndex = 0;

pthread_mutex_t mutexParking = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexEntrance = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condEmpty = PTHREAD_COND_INITIALIZER;



struct carInfo
{
  char direction;
  int number;
  clock_t parkingTime;
  char fifoName[15];
};

void alarm_handler(int signo)
{
  closingTime = 1;
  int i=0;
  for(;i<NUM_ENTRANCES; i++){
    sem_post(semaphores[i]);
  }
}

void updateLog(){

}

void *janitor(void *arg){
  pthread_t ownThread = pthread_self();
  if(pthread_detach(ownThread) != 0)
  {
    printf("Error making thread nr %d detached\n", (int)ownThread);
    exit(1);
  }

  struct carInfo car = *(struct carInfo*) arg;
  int fifofd;
  // printf("%s\n", car.fifoName);

  //do {
    fifofd = open(car.fifoName, O_WRONLY);
  //} while(fifofd == -1);
  pthread_mutex_lock(&mutexParking);
  if(parkingSpaces == 0){
    write(fifofd, FULL, sizeof(FULL));
    printf("full: %d - left:%d\n", car.number, parkingSpaces);
    pthread_mutex_unlock(&mutexParking);
    updateLog();
    return NULL;
  }
  parkingSpaces--;
  write(fifofd, IN, sizeof(IN));
  printf("in: %d - left:%d\n", car.number, parkingSpaces);
  pthread_mutex_unlock(&mutexParking);
  updateLog();

  clock_t start, end;
  start = clock();
  do{
    end = clock();
  }while(end-start >= car.parkingTime);

  pthread_mutex_lock(&mutexParking);
  parkingSpaces++;
  printf("out: %d - left:%d\n", car.number, parkingSpaces);
  pthread_mutex_unlock(&mutexParking);

   write(fifofd, OUT, sizeof(OUT));
  close(fifofd);
  unlink(car.fifoName);

  if(closingTime == 1 && parkingSpaces == maxSpaces){
    pthread_cond_broadcast(&condEmpty);
  }
  updateLog();


  return NULL;
}


void *entrancePoints(void *arg)
{
  char fifoName[15], semName[15] ;
  sprintf(fifoName, "fifo%c", *(char *)arg);
  sprintf(semName, "%s%c", SEM_NAME, *(char *)arg);
  printf("Controller: %s   %s\n", fifoName, semName);
  int fifofd;

  mkfifo(fifoName, 0660);
  fifofd = open(fifoName, O_RDONLY | O_NONBLOCK);
  printf("%d\n", fifofd);

  pthread_mutex_lock(&mutexEntrance);
  int a = semIndex++;
  semaphores[a] = sem_open(semName,O_CREAT,0600,0);
  pthread_mutex_unlock(&mutexEntrance);

  if(semaphores[a] == SEM_FAILED){
    printf("Error opening semaphore\n");
    exit(4);
  }
  struct carInfo car;
  int n;
  while (1){
    sem_wait(semaphores[a]);
    //printf("%d\n", parkingSpaces);

    n = read(fifofd, &car, sizeof(struct carInfo));
    if(n > 0){
      //printf("%s  car: %c%d - time: %d\n", semName, car.direction, car.number, (int)car.parkingTime);
    }
    if(n == 0 && closingTime == 0){
      continue;
    }
    if(closingTime == 1){

      close(fifofd);
      /*while(n > 0){
        n = read(fifofd, &car, sizeof(struct carInfo));
        printf("timeout car: %c%d - time: %d\n", car.direction, car.number, (int)car.parkingTime);
      }*/
      sem_close(semaphores[a]);

      unlink(fifoName);
      pthread_mutex_lock(&mutexParking);
      while(parkingSpaces != maxSpaces)
        pthread_cond_wait(&condEmpty, &mutexParking);
      pthread_mutex_unlock(&mutexParking);
      return NULL;
    }


    pthread_t janitorThread;
    pthread_create(&janitorThread, NULL, janitor, &car);
  }

}

int main(int argc, char* argv[]){

  if(argc != 3) {
    printf("Usage: parque <N_LUGARES> <T_ABERTURA>\n");
    exit(1);
  }
  unsigned int workingTime;
  workingTime = strtol(argv[2],NULL,10);
  parkingSpaces = strtol(argv[1], NULL, 10);

  if(workingTime <= 0 || parkingSpaces <= 0) {
    printf("Illegal arguments. Use positive <unsigned int> <unsigned int>\n");
    exit(2);
  }
  maxSpaces = parkingSpaces;

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
  printf("%d\n", parkingSpaces);
  return 0;
}
