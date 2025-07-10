#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(char argc,char* argv[])
{
    if(fork() > 0)
    sleep(5);  // Let child exit before parent.
  exit(0);
    exit(0);
}
