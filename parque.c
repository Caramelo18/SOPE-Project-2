#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>


unsigned int parkingSpaces;
unsigned int maxSpaces;

#define NUM_ENTRANCES   4

void alarm_handler(int signo){
        printf("Time's up %d\n", signo);
      exit(0);
}

void *entrancePoints(void *arg){
        char fifoName[6]; //tipo nao era preciso fazer tao grande porque sabes que vai ser so "fifoN"
        sprintf(fifoName, "fifo%s", (char *)arg);
        printf("Controller: %s\n", fifoName);
        int fd;
        mkfifo(fifoName, 0660);
        fd = open(fifoName, O_RDONLY);
        close(fd);
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
        //alarm(1);

        //while(1) ;


        pthread_t threads[NUM_ENTRANCES];
        char *fifoNames[NUM_ENTRANCES]= {"N", "S", "E", "W"};
        int i;
        for (i = 0; i < NUM_ENTRANCES; i++) {
                pthread_create(&threads[i], NULL, entrancePoints, fifoNames[i]);

        }
        pthread_exit(0);
        return 0;
}
