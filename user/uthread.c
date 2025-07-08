#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define MAX_THREADS 3
#define STACK_SIZE 8192
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
extern void switch_thread(struct context* old,struct context* new);
enum STATE
    {
        FREE=0,
        RUNNABLE,
        RUNNING
        
    };
struct thread
{
    
    enum STATE state;
    struct context context;
    char stack[STACK_SIZE];
};
struct thread threads[MAX_THREADS];
struct thread* current_thread;
struct context main_context;
volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;
void init_thread()
{
    
    
    
}
void schedule_thread()
{
    struct thread* th;
    int flag=0;
    for(;;)
    {
        flag=0;
        for(th=threads;th<threads+MAX_THREADS;++th)
        {
            if(th->state==RUNNABLE)
            {
                flag=1;
                th->state=RUNNING;
                current_thread=th;
                switch_thread(&main_context,&th->context);
                current_thread=0;
            }
        }
        if(flag==0)
        {
            printf("no runnable thread\n");
            exit(0);
        }
    }
}
void create_thread(void (*func)())
{
    struct thread* th;
    for(th=threads;th<threads+MAX_THREADS;++th)
    {
        if(th->state==FREE)
        {
            th->state=RUNNABLE;
            (th->context).ra=(uint64)func;
            (th->context).sp=(uint64)(th->stack+STACK_SIZE);
            return;
        }
    }
    printf("no free thread\n");
    exit(1);
}
void yield_thread()
{
    current_thread->state=RUNNABLE;
    switch_thread(&current_thread->context,&main_context);
}
void 
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)
    yield_thread();
  
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    yield_thread();
  }
  printf("thread_a: exit after %d\n", a_n);

  current_thread->state = FREE;
  schedule_thread();
}

void 
thread_b(void)
{
  int i;
  printf("thread_b started\n");
  b_started = 1;
  while(a_started == 0 || c_started == 0)
    yield_thread();
  
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);
    b_n += 1;
    yield_thread();
  }
  printf("thread_b: exit after %d\n", b_n);

  current_thread->state = FREE;
  schedule_thread();
}

void 
thread_c(void)
{
  int i;
  printf("thread_c started\n");
  c_started = 1;
  while(a_started == 0 || b_started == 0)
    yield_thread();
  
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);
    c_n += 1;
    yield_thread();
  }
  printf("thread_c: exit after %d\n", c_n);

  current_thread->state = FREE;
  schedule_thread();
}
int main(char argc,char* argv[])
{
   // printf("uthread\n");
    a_started = b_started = c_started = 0;
     //printf("uthread\n");
    a_n = b_n = c_n = 0;
    //printf("uthread\n");
    init_thread();
    create_thread(thread_a);
    create_thread(thread_b);
    create_thread(thread_c);
    schedule_thread();
    exit(-1);
}