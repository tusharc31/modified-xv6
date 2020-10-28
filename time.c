// Simple grep.  Only supports ^ . * $ operators.

#include "types.h"
#include "stat.h"
#include "user.h"

char buf[1024];
int match(char*, char*);

int main(int argc, char *argv[])
{
  if(argc <= 1){
    printf(2, "less no of arguments.");
    exit();
  }
  int x=fork();
  if(x==0) {
	  exec(argv[1], argv+1);
	  printf(2,"Error");
	  exit();
  }
  int wtime,rtime;
  while(waitx(&wtime, &rtime)<=0);
  printf(1, "wait time: %d\nruntime: %d\n", wtime, rtime);
  exit();
}
