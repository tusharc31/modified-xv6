// Simple grep.  Only supports ^ . * $ operators.

#include "types.h"
#include "stat.h"
#include "user.h"

char buf[1024];
int match(char*, char*);

int main(int argc, char *argv[])
{
  if(argc != 3){
    printf(2, "less no of arguments.");
    exit();
  }
  int new_priority = atoi(argv[1]);
  int pid = atoi(argv[2]);
  if(new_priority < 0 || pid<0)
{
	printf(2,"Only positive integer arguments");
	exit();
}
int x=set_priority(new_priority, pid);
if(x>=0)
	printf(1, "Old priority: %d", x);
else
printf(2, "Some error");
exit();
}
