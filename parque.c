#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

int workingTime;
int parkingSpaces;

#define NUM_CONTROLLER   4


void *entrancePoints(void *args)
{
    printf("Controller: %s\n", (char *) args);
    return NULL;
}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        printf("Usage: parque <N_LUGARES> <T_ABERTURA>\n");
        exit(-1);
    }

    parkingSpaces = strtol(argv[1], NULL, 10);
    workingTime = strtol(argv[2],NULL,10);

    printf("%d %d\n", parkingSpaces, workingTime);

    pthread_t threads[NUM_CONTROLLER];
    char *fifoNames[NUM_CONTROLLER]= {"N", "S", "E", "W"};

    int i;
    for (i = 0; i < NUM_CONTROLLER; i++)
    {
        pthread_create(&threads[i], NULL, entrancePoints, (void *)fifoNames[i]);
    }
    
    pthread_exit(0);
    return 0;
}
