#define _POSIX_C_SOURCE 199309L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#define ROUND 10000
int npth;
struct barrier
{
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int round;
    int nth;
}bstate;
void bstate_init()
{
    pthread_mutex_init(&bstate.mtx,NULL);
    pthread_cond_init(&bstate.cond,NULL);
    bstate.round=0;
    bstate.nth=0;
}
void msleep(unsigned int milliseconds) 
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}
void usleep(unsigned int microseconds)
{
    struct timespec ts;
    ts.tv_sec=microseconds/1000000;
    ts.tv_nsec=(microseconds%1000000)*1000;
    nanosleep(&ts,NULL);
}
void barrier()
{
    pthread_mutex_lock(&bstate.mtx);
    if(++bstate.nth<npth)
        pthread_cond_wait(&bstate.cond,&bstate.mtx);
    else
    {
        bstate.nth=0;
        ++bstate.round;
        pthread_cond_broadcast(&bstate.cond);
    }
    pthread_mutex_unlock(&bstate.mtx);
}
void* work(void* arg)
{
    for(int i=0;i<ROUND;i++)
    {
        assert(i==bstate.round);
        barrier();
        
        usleep(rand()%100);
        
    }
}
int main(char argc,char* argv[])
{
    if(argc<2)
    {
        printf("usage: barrier <number>\n");
        return -1;
    }
    
    pthread_t* pths;
    void* rt_val=NULL;

    npth=atoi(argv[1]);

    pths=(pthread_t*)malloc(sizeof(pthread_t)*npth);
    
    srand(time(NULL));

    bstate_init();

    for(int i=0;i<npth;i++)
    {
        assert(pthread_create(&pths[i],NULL,work,NULL)==0);
    }
    for(int i=0;i<npth;i++)
    {
        assert(pthread_join(pths[i],&rt_val)==0);
    }
    printf("barrier pass!\n");
    return 0;
}