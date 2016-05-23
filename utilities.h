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

#define NUM_ENTRANCES     4
#define CONTROLLER_INFO   7
#define FIFO_MAX_NAME     15
#define FULL              "FULL"
#define OUT               "OUT"
#define IN                "IN"
#define CLOSED            "CLOSED"

const char SEM_NAME[] = "/sem";
const char fifoNames[NUM_ENTRANCES]= {'N', 'S', 'E', 'W'};

struct carInfo
{
    char direction;
    int number;
    clock_t parkingTime;
    char fifoName[FIFO_MAX_NAME];
};
