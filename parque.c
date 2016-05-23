#include "utilities.h"

#define LOG             "parque.log"

unsigned int maxSpaces;

unsigned int parkingSpaces;
short closingTime = 0;
sem_t *semaphores[NUM_ENTRANCES];
int semIndex = 0;

pthread_mutex_t mutexParking = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexEntrance = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexLog = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condEmpty = PTHREAD_COND_INITIALIZER;


void alarm_handler(int signo)
{
    closingTime = 1;
    int i=0;
    for(; i<NUM_ENTRANCES; i++)
        sem_post(semaphores[i]);
}

void updateLog(int id, char message[])
{
    FILE *file;
    clock_t c = clock();
    file = fopen(LOG, "a");
    fprintf(file,"%7d  ; %4d ; %5d   ; %s\n", (int)c, parkingSpaces, id, message);
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
    int fifofd;

    fifofd = open(car->fifoName, O_WRONLY);

    pthread_mutex_lock(&mutexParking);

    if(closingTime == 1) {
        updateLog(car->number, CLOSED);
        write(fifofd, CLOSED, strlen(CLOSED));
    }
    else {
        if(parkingSpaces == 0) {
            updateLog(car->number, FULL);
            write(fifofd, FULL, sizeof(FULL));
        }
        else{
            parkingSpaces--;
            updateLog(car->number, IN);
            pthread_mutex_unlock(&mutexParking);
            write(fifofd, IN, strlen(IN));

            clock_t start, end;
            start = clock();
            do {
                    end = clock();
            } while(end-start <= car->parkingTime);

            pthread_mutex_lock(&mutexParking);
            parkingSpaces++;
            updateLog(car->number, OUT);
            pthread_mutex_unlock(&mutexParking);

            write(fifofd, OUT, strlen(OUT));
            unlink(car->fifoName);

            pthread_mutex_lock(&mutexParking);
            if(closingTime == 1 && parkingSpaces == maxSpaces) {
                pthread_cond_broadcast(&condEmpty);
            }
        }
    }
    pthread_mutex_unlock(&mutexParking);
    close(fifofd);
    free(car);
    return NULL;
}


void *entrancePoints(void *arg)
{
    char fifoName[CONTROLLER_INFO], semName[CONTROLLER_INFO];
    sprintf(fifoName, "fifo%c", *(char *)arg);
    sprintf(semName, "%s%c", SEM_NAME, *(char *)arg);
    printf("Controller: %-10s %-10s\n", fifoName, semName);

    int fifofd;
    mkfifo(fifoName, 0660);
    if((fifofd = open(fifoName, O_RDONLY | O_NONBLOCK)) == -1) {
        printf("Error opening %s errno: %d\n", fifoName, errno);
        exit(4);
    }

    pthread_mutex_lock(&mutexEntrance);
    int a = semIndex++;
    semaphores[a] = sem_open(semName,O_CREAT,0660,0);
    pthread_mutex_unlock(&mutexEntrance);

    if(semaphores[a] == SEM_FAILED) {
        printf("Error opening semaphore\n");
        exit(5);
    }

    while (1) {
        if(sem_wait(semaphores[a]) == -1){
            if(errno != EINVAL)
                printf("Error ocurred during %s wait. Errno: %d\n", semName, errno);
                return NULL;
        }
        if(closingTime == 1){
            sem_close(semaphores[a]);
            sem_unlink(semName);
        }

        struct carInfo *vehicle = malloc(sizeof(struct carInfo));
        if(read(fifofd, vehicle, sizeof(struct carInfo)) > 0){
            pthread_t janitorThread;
            pthread_create(&janitorThread, NULL, janitor, vehicle);
        }
        else if(closingTime == 1){
            pthread_mutex_lock(&mutexParking);
            while(parkingSpaces != maxSpaces)
                pthread_cond_wait(&condEmpty, &mutexParking);
            pthread_mutex_unlock(&mutexParking);
            close(fifofd);
            unlink(fifoName);
            return NULL;

        }
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
        exit(1);
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
        exit(1);
    }

    alarm(workingTime);
    pthread_t threads[NUM_ENTRANCES];

    int i;
    for (i = 0; i < NUM_ENTRANCES; i++)
        if(pthread_create(&threads[i], NULL, entrancePoints, (void *)&fifoNames[i]) == -1){
            printf("Error creating controller thread. Errno: %d\n", errno);
            exit(1);
        }

    for (i = 0; i < NUM_ENTRANCES; i++)
        if(pthread_join(threads[i], NULL) == -1)
            printf("Error ocurred during pthread_join\n");
    return 0;
}
