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
pthread_cond_t condEmpty = PTHREAD_COND_INITIALIZER;


void alarm_handler(int signo)
{
  closingTime = 1;
  int i=0;
  for(;i<NUM_ENTRANCES; i++){
    sem_post(semaphores[i]);
  }
}

void updateLog(int id, char message[])
{
  FILE *file;
  clock_t c = clock();
  file = fopen(LOG, "a");
  char loga[40];
  sprintf(loga,"%7d  ; %4d ; %5d  ; %s\n", (int)c, parkingSpaces, id, message);
  //fwrite(file, loga, strlen(loga));
  fclose(file);
  return;
}


void *janitor(void *arg){
  pthread_t ownThread = pthread_self();
  if(pthread_detach(ownThread) != 0)
  {
    printf("Error making thread nr %d detached\n", (int)ownThread);
    exit(1);
  }

  struct carInfo *car = arg;

  char message [35];
  int fifofd;

  fifofd = open(car->fifoName, O_WRONLY);
  //write(STDOUT_FILENO, car->fifoName, strlen(car->fifoName));
  //write(STDOUT_FILENO, "\n", 2);

  pthread_mutex_lock(&mutexParking);
  if(parkingSpaces == 0){
    sprintf(message, "full: %d - left:%d\n", car->number, parkingSpaces);
    write(STDOUT_FILENO,message, strlen(message));
    updateLog(car->number, FULL);
    pthread_mutex_unlock(&mutexParking);
    write(fifofd, FULL, sizeof(FULL));
    close(fifofd);
    free(car);
    return NULL;
  }
  parkingSpaces--;
  //clock_t c = clock();
  sprintf(message, " in: %c%d - left:%d\n", car->direction, car->number, parkingSpaces);
  write(STDOUT_FILENO,message, strlen(message));
  updateLog(car->number, IN);
  pthread_mutex_unlock(&mutexParking);
  write(fifofd, IN, sizeof(IN));
  //close(fifofd);

  clock_t start, end;
  start = clock();
  do{
    end = clock();
  }while(end-start <= car->parkingTime);


  pthread_mutex_lock(&mutexParking);
  parkingSpaces++;

  //clock_t o = clock();
  sprintf(message, "out: %c%d - left:%d\n", car->direction, car->number, parkingSpaces);

  updateLog(car->number, OUT);

  write(STDOUT_FILENO,message, strlen(message));
  pthread_mutex_unlock(&mutexParking);

  //fifofd = open(car->fifoName, O_WRONLY);
  if(write(fifofd, OUT, sizeof(OUT)) >0){
    //printf("%s %s\n", car->fifoName, OUT);
  }
  close(fifofd);
  unlink(car->fifoName);

  free(car);
  if(closingTime == 1 && parkingSpaces == maxSpaces){
    pthread_cond_broadcast(&condEmpty);
  }


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

  pthread_mutex_lock(&mutexEntrance);
  int a = semIndex++;
  semaphores[a] = sem_open(semName,O_CREAT,0660,0);
  pthread_mutex_unlock(&mutexEntrance);

  if(semaphores[a] == SEM_FAILED){
    printf("Error opening semaphore\n");
    exit(5);
  }

  int n;
  while (1){
    sem_wait(semaphores[a]);

    if(closingTime == 1)
    {
      close(fifofd);
      sem_close(semaphores[a]);
      unlink(fifoName);
      pthread_mutex_lock(&mutexParking);
      while(parkingSpaces != maxSpaces)
        pthread_cond_wait(&condEmpty, &mutexParking);
      pthread_mutex_unlock(&mutexParking);
      return NULL;
    }

    struct carInfo *vehicle = malloc(sizeof(struct carInfo));

    n = read(fifofd, vehicle, sizeof(struct carInfo));
    if(n <= 0 && closingTime == 0){
      continue;
    }

    pthread_t janitorThread;
    pthread_create(&janitorThread, NULL, janitor, vehicle);
  }

}

int main(int argc, char* argv[])
{

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
  FILE *parque = fopen(LOG, "w");
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
