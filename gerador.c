#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#define NUM_ENTRANCES   4

unsigned int durationPeriod;
unsigned int minInterval;



void alarm_handler(int signo){
        printf("Time's up %d\n", signo);
      exit(0);
}


int main(int argc, char* argv[]){

  if(argc != 3) {
          printf("Usage: gerador <T_GERACAO> <U_RELOGIO>\n");
          exit(1);
  }
  durationPeriod = strtol(argv[1], NULL, 10);
  minInterval = strtol(argv[2],NULL,10);


  if(durationPeriod <= 0 || minInterval <= 0) {
          printf("Illegal arguments. Use <unsigned int> <unsigned int>\n");
          exit(2);
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
          exit(3);
  }
  alarm(durationPeriod);

  char *fifoNames[NUM_ENTRANCES]= {"N", "S", "E", "W"};
  int index = rand() % 4;
  char *direction = fifoNames[index];


}
