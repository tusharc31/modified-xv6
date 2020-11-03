
#include "types.h"
#include "mmu.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "proc.h"
#include "x86.h"

char buf[1024];
int match(char*, char*);

int main(int argc, char *argv[])
{
  if(argc < 1 || !argv){
    printf(2, "less no of arguments.");
    exit();
  }
  pscall();
  exit();
}
