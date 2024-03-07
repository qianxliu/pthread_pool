#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef void ThreadRet;
struct ThreadArgs;

struct pool_queue
{
    struct ThreadArgs *args;
    char free;
    struct pool_queue *next;
};

struct pool
{
    char cancelled;
    ThreadRet (*fn)(struct ThreadArgs *);
    unsigned int remaining;
    unsigned int nthreads;
    struct pool_queue *q;
    struct pool_queue *end;
    pthread_mutex_t q_mtx;
    pthread_cond_t q_cnd;
    pthread_t threads[1];
};

static ThreadRet* thread(struct ThreadArgs *args);

struct pool* pool_create(ThreadRet (*thread_func)(struct ThreadArgs *), unsigned int threads) {
    struct pool* p = (struct pool *) malloc(sizeof(struct pool) + (threads-1) * sizeof(pthread_t));

    pthread_mutex_init(&p->q_mtx, NULL);
    pthread_cond_init(&p->q_cnd, NULL);
    p->nthreads = threads;
    p->fn = thread_func;
    p->cancelled = 0;
    p->remaining = 0;
    p->end = NULL;
    p->q = NULL;

    for (int i = 0; i < threads; ++i) {
        pthread_create(&p->threads[i], NULL, (void *)&thread, p);
    }

    return p;
}

void pool_enqueue(struct pool* pool, struct ThreadArgs *args, char free) {
    struct pool_queue *q = (struct pool_queue *) malloc(sizeof(struct pool_queue));
    q->args = args;
    q->next = NULL;
    q->free = free;

    pthread_mutex_lock(&pool->q_mtx);
    if (pool->end != NULL) pool->end->next = q;
    if (pool->q == NULL) pool->q = q;
    pool->end = q;
    pool->remaining++;
    pthread_cond_signal(&pool->q_cnd);
    pthread_mutex_unlock(&pool->q_mtx);
}

void pool_wait(struct pool* pool) {
    pthread_mutex_lock(&pool->q_mtx);
    while (!pool->cancelled && pool->remaining)
    {
        pthread_cond_wait(&pool->q_cnd, &pool->q_mtx);
    }
    pthread_mutex_unlock(&pool->q_mtx);
}

void pool_end(struct pool* pool) {
    pool->cancelled = 1;

    pthread_mutex_lock(&pool->q_mtx);
    pthread_cond_broadcast(&pool->q_cnd);
    pthread_mutex_unlock(&pool->q_mtx);

    for (int i = 0; i < pool->nthreads; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }

    struct pool_queue *q;
    while (pool->q != NULL)
    {
        q = pool->q;
        pool->q = q->next;

        if (q->free) free(q->args);
        free(q);
    }
    free(pool);
}

ThreadRet* thread(struct ThreadArgs *args)
{
    struct pool *p = (struct pool *) args;
    struct pool_queue *q;

    while (!p->cancelled)
    {
        pthread_mutex_lock(&p->q_mtx);
        while (!p->cancelled && p->q == NULL)
        {
            pthread_cond_wait(&p->q_cnd, &p->q_mtx);
        }
        if (p->cancelled)
        {
            pthread_mutex_unlock(&p->q_mtx);
            return NULL;
        }
        q = p->q;
        p->q = q->next;
        p->end = (q == p->end ? NULL : p->end);
        pthread_mutex_unlock(&p->q_mtx);

        p->fn(q->args);

        if (q->free) free(q->args);
        free(q);
        q = NULL;

        pthread_mutex_lock(&p->q_mtx);
        --p->remaining;
        pthread_cond_broadcast(&p->q_cnd);
        pthread_mutex_unlock(&p->q_mtx);
    }

    return NULL;
}

typedef void ThreadRet;

typedef struct ThreadArgs {
    int n;
    int init;
    int times;
} ThreadArgs;

#include<math.h>

int is_prime(int n)
{
    if (n < 2)
        return -1;
    for (int i = 2; i <= sqrt(n); ++i)
    {
        if ((n % i) == 0)
            return -1;
    }
    return n;
}

void thread_prime(ThreadArgs *args)
{
    for (int i = args->init; i <= args->n; i += args->times)
    {
        if (is_prime(i) >= 2)
            printf("%d\t", i);
    }
}

#include<time.h>

int main()
{
    int N = 3e7;
    // clock() is for cumulative CPU ticks so we cannot use it for multithreading testing
    // it will always be longer than single thread.
    struct timespec start, end;
    double s1, s2;

    clock_gettime(CLOCK_REALTIME, &start);
    for (int i = 1; i <= N; i+=1)
    {
        if (is_prime(i) >= 2)
            printf("%d\t", i);
    }
    clock_gettime(CLOCK_REALTIME, &end);
    s1 = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    struct pool* p;
    // siebenundfünfzig/fifty-seven/cinquante sept. Salut for the mistake from an old student.
    // who failed some classes when he was an undergraduate.
    int max_threads = sqrt(N)/57;
    int current = max_threads;
    p = pool_create(thread_prime, max_threads);
    while (current--)
    {
        ThreadArgs *args = (ThreadArgs *)malloc(sizeof (ThreadArgs));
        args->n = N;
        args->init = current;
        args->times = max_threads;
        pool_enqueue(p, args, 0);
    }
    clock_gettime(CLOCK_REALTIME, &start);
    pool_wait(p);
    pool_end(p);
    clock_gettime(CLOCK_REALTIME, &end);
    s2 = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\nExecution time1: %f s\n",s1);
    printf("\nExecution time2: %f s\n",s2);
}
