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
#define LOG             "parque.log"



const char SEM_NAME[] = "/sem";
char fifoNames[NUM_ENTRANCES]= {'N', 'S', 'E', 'W'};
//const char PRIVATE_FIFO[] = "./temp/fifo";

struct carInfo
{
  char direction;
  int number;
  clock_t parkingTime;
  char fifoName[15];
};

//char FIFO_BACKUP[FIFO_SIZE];
int maxSpaces;

unsigned int parkingSpaces;
unsigned int closingTime = 0;
sem_t *semaphores[4];
int semIndex = 0;

pthread_mutex_t mutexParking = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexEntrance = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexLog = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condEmpty = PTHREAD_COND_INITIALIZER;





void alarm_handler(int signo)
{
  closingTime = 1;
  int i=0;
  for(;i<NUM_ENTRANCES; i++){
    sem_post(semaphores[i]);
  }
}

void updateLog(int id, char message[]){
  pthread_mutex_lock(&mutexLog);
  FILE *file;
  clock_t c = clock();
  file = fopen(LOG, "a");
  fprintf(file,"%7d  ; %5d ; %5d  ; %s\n", (int)c, parkingSpaces, id, message);
  fclose(file);
  pthread_mutex_unlock(&mutexLog);
}


void *janitor(void *arg){
  pthread_t ownThread = pthread_self();
  if(pthread_detach(ownThread) != 0)
  {
    printf("Error making thread nr %d detached\n", (int)ownThread);
    exit(1);
  }

  struct carInfo *car = arg;

  int fifofd;
  //printf("%s\n", car->fifoName);

  //do {
    fifofd = open(car->fifoName, O_WRONLY);
  //} while(fifofd == -1);
  pthread_mutex_lock(&mutexParking);
  if(parkingSpaces == 0){
    printf("full: %d - left:%d\n", car->number, parkingSpaces);
    pthread_mutex_unlock(&mutexParking);
    write(fifofd, FULL, sizeof(FULL));
    updateLog(car->number, FULL);
    close(fifofd);
    free(car);
    return NULL;
  }
  parkingSpaces--;
  printf("in: %c%d - left:%d\n", car->direction, car->number, parkingSpaces);
  pthread_mutex_unlock(&mutexParking);
  write(fifofd, IN, sizeof(IN));
  updateLog(car->number, IN);

  clock_t start, end;
  start = clock();
  do{
    end = clock();
  }while(end-start >= car->parkingTime);


  pthread_mutex_lock(&mutexParking);
  parkingSpaces++;
  write(fifofd, OUT, sizeof(OUT));
  printf("out: %d - left:%d\n", car->number, parkingSpaces);
  pthread_mutex_unlock(&mutexParking);

  close(fifofd);
  unlink(car->fifoName);

  if(closingTime == 1 && parkingSpaces == maxSpaces){
    pthread_cond_broadcast(&condEmpty);
  }
  updateLog(car->number, OUT);

  free(car);
  return NULL;
}


void *entrancePoints(void *arg)
{
  char fifoName[8], semName[8] ;
  sprintf(fifoName, "fifo%c", *(char *)arg);
  sprintf(semName, "%s%c", SEM_NAME, *(char *)arg);
  printf("Controller: %s   %s\n", fifoName, semName);
  int fifofd;

  mkfifo(fifoName, 0660);

  if((fifofd = open(fifoName, O_RDONLY | O_NONBLOCK)) == -1){
    printf("Error opening %s\n", fifoName);
    exit(4);
  }
  //printf("%d\n", fifofd);

  pthread_mutex_lock(&mutexEntrance);
  int a = semIndex++;
  semaphores[a] = sem_open(semName,O_CREAT,0600,0);
  pthread_mutex_unlock(&mutexEntrance);

  if(semaphores[a] == SEM_FAILED){
    printf("Error opening semaphore\n");
    exit(5);
  }
  //struct carInfo car;
  int n;
  while (1){
    sem_wait(semaphores[a]);
    //printf("%d\n", i);
    //printf("%d\n", parkingSpaces);

    if(closingTime == 1){
      close(fifofd);
      /*while(n > 0){
        n = read(fifofd, &car, sizeof(struct carInfo));
        printf("timeout car: %c%d - time: %d\n", car.direction, car.number, (int)car.parkingTime);
      }*/
      sem_close(semaphores[a]);

      unlink(fifoName);
      //printf("closing time\n");
      //free(vehicle);
      //printf("%s - %p\n", fifoName, vehicle);
      pthread_mutex_lock(&mutexParking);
      while(parkingSpaces != maxSpaces)
        pthread_cond_wait(&condEmpty, &mutexParking);
      pthread_mutex_unlock(&mutexParking);
      return NULL;
    }

    struct carInfo *vehicle = malloc(sizeof(struct carInfo));

    n = read(fifofd, vehicle, sizeof(struct carInfo));
    if(n > 0){
      //printf("%s  car: %c%d - time: %d\n", semName, vehicle->direction, vehicle->number, (int)vehicle->parkingTime);
    }
    if(n <= 0 && closingTime == 0){
      continue;
    }

    pthread_t janitorThread;
    pthread_create(&janitorThread, NULL, janitor, vehicle);
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

  // resets parque.log
  FILE *parque = fopen("parque.log", "w");
  fprintf(parque, "t(ticks) ; nlug ; id_viat ; observ\n");
  fclose(parque);

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


  int i;
  for (i = 0; i < NUM_ENTRANCES; i++)
    pthread_create(&threads[i], NULL, entrancePoints, &fifoNames[i]);

  for (i = 0; i < NUM_ENTRANCES; i++)
    pthread_join(threads[i], NULL);
  printf("%d\n", parkingSpaces);
  return 0;
}
