#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>

#define NKEYS 10000
#define BUCKET 5
struct entry
{
    int key;
    int val;
    struct entry *next;
};
struct entry *table[BUCKET];
pthread_mutex_t table_locks[BUCKET];
int keys[NKEYS];
int pth_num;
void put(int key, int val)
{
    struct entry *e;
    int k = key % BUCKET;
    pthread_mutex_lock(&table_locks[k]);
    for (e = table[k]; e != NULL; e = e->next)
    {
        if (e->key == key)
        {
            e->val = val;
            break;
        }
    }
    if (!e)
    {
        e = (struct entry *)malloc(sizeof(struct entry));
        e->key = key;
        e->val = val;
        e->next=table[k];
        table[k]=e;
    }
    pthread_mutex_unlock(&table_locks[k]);

}
int get(int key)
{
    struct entry *e = NULL;
    int k = key % BUCKET;
    for (e = table[k]; e != NULL; e = e->next)
    {
        if (e->key == key)
            break;
    }
    return e;
}
void *put_keys(void *pth_id)
{
    long id = (long)pth_id;
    int key_num = NKEYS / pth_num;
    for (int i = id * key_num; i < (id + 1) * key_num; i++)
    {
        put(keys[i], id);
    }
    return NULL;
}
void *get_keys(void *pth_id)
{
    long id = (long)pth_id;
    int missing = 0;
    for (int i = 0; i < NKEYS; i++)
    {
        if (get(keys[i]) == 0)
            ++missing;
    }
    printf("ph%d : missing %d\n", id, missing);
    return NULL;
}
double now()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}
int main(char argc, char *argv[])
{
    if (argc < 2)
    {
        printf("ph <number>\n");
        return -1;
    }

    pthread_t *pthreads;
    void *re_val = NULL;
    double start, end;

    pth_num = atoi(argv[1]);

    assert(NKEYS % pth_num == 0);

    pthreads = (pthread_t *)malloc(sizeof(pthread_t) * pth_num);

    srand(time(NULL));
    for (int i = 0; i < NKEYS; i++)
    {
        keys[i] = rand();
    }

    for (int i = 0; i < BUCKET; i++)
    {
        pthread_mutex_init(&table_locks[i], NULL);
    }
    start = now();
    for (int i = 0; i < pth_num; i++)
    {
        assert(pthread_create(&pthreads[i], NULL, put_keys, (void *)i) == 0);
    }
    for (int i = 0; i < pth_num; i++)
    {
        assert(pthread_join(pthreads[i], &re_val) == 0);
    }
    end = now();
    printf("%d puts, %.3f seconds, %.0f puts/second\n",
           NKEYS, end - start, NKEYS / (end - start));

    for (int i = 0; i < pth_num; i++)
    {
        assert(pthread_create(&pthreads[i], NULL, get_keys, (void *)i) == 0);
    }
    for (int i = 0; i < pth_num; i++)
    {
        assert(pthread_join(pthreads[i], &re_val) == 0);
    }
    printf("%d gets, %.3f seconds, %.0f gets/second\n",
           NKEYS * pth_num, end - start, NKEYS * pth_num / (end - start));

    return 0;
}