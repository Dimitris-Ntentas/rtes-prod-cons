#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define QUEUESIZE 10
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 5
#define NUM_TASKS 100

typedef struct {
  void *(*work)(void *);
  void *arg;
  struct timeval time_enqueued;
} workFunction;

typedef struct {
  workFunction buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit(void);
void queueDelete(queue *q);
void queueAdd(queue *q, workFunction in);
void queueDel(queue *q, workFunction *out);

void *producer(void *args);
void *consumer(void *args);
void *taskFunction(void *arg);

int main() {
  queue *fifo;
  pthread_t pro[NUM_PRODUCERS], con[NUM_CONSUMERS];

  fifo = queueInit();
  if (fifo == NULL) {
    fprintf(stderr, "Queue Init failed.\n");
    exit(1);
  }

  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pthread_create(&pro[i], NULL, producer, fifo);
  }
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    pthread_create(&con[i], NULL, consumer, fifo);
  }

  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pthread_join(pro[i], NULL);
  }
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    pthread_join(con[i], NULL);
  }

  queueDelete(fifo);

  return 0;
}

void *producer(void *q) {
  queue *fifo = (queue *)q;
  workFunction wfn;

  for (int i = 0; i < NUM_TASKS; i++) {
    wfn.work = taskFunction;
    wfn.arg = malloc(sizeof(int));  // Example task: calculating sine of an angle
    *(int *)(wfn.arg) = i;         // Assign task an angle or similar simple value

    pthread_mutex_lock(fifo->mut);
    while (fifo->full) {
      pthread_cond_wait(fifo->notFull, fifo->mut);
    }
    gettimeofday(&wfn.time_enqueued, NULL);
    queueAdd(fifo, wfn);
    pthread_cond_signal(fifo->notEmpty);
    pthread_mutex_unlock(fifo->mut);
  }
  return NULL;
}

void *consumer(void *q) {
  queue *fifo = (queue *)q;
  workFunction wfn;
  struct timeval now;
  long delay;

  while (1) {
    pthread_mutex_lock(fifo->mut);
    while (fifo->empty) {
      pthread_cond_wait(fifo->notEmpty, fifo->mut);
    }
    queueDel(fifo, &wfn);
    pthread_cond_signal(fifo->notFull);
    pthread_mutex_unlock(fifo->mut);

    gettimeofday(&now, NULL);
    delay = (now.tv_sec - wfn.time_enqueued.tv_sec) * 1000000L + (now.tv_usec - wfn.time_enqueued.tv_usec);
    printf("Task delay: %ld microseconds.\n", delay);

    wfn.work(wfn.arg);
    free(wfn.arg);
  }
  return NULL;
}

void *taskFunction(void *arg) {
  int num = *(int *)arg;
  printf("Executing task for input %d.\n", num);
  return NULL;
}

queue *queueInit(void) {
  queue *q;

  q = (queue *)malloc(sizeof(queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(q->mut, NULL);
  q->notFull = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(q->notEmpty, NULL);

  return q;
}

void queueDelete(queue *q) {
  pthread_mutex_destroy(q->mut);
  free(q->mut);
  pthread_cond_destroy(q->notFull);
  free(q->notFull);
  pthread_cond_destroy(q->notEmpty);
  free(q->notEmpty);
  free(q);
}

void queueAdd(queue *q, workFunction in) {
  q->buf[q->tail] = in;
  q->tail++;
  if (q->tail == QUEUESIZE) q->tail = 0;
  if (q->tail == q->head) q->full = 1;
  q->empty = 0;
}

void queueDel(queue *q, workFunction *out) {
  *out = q->buf[q->head];
  q->head++;
  if (q->head == QUEUESIZE) q->head = 0;
  if (q->head == q->tail) q->empty = 1;
  q->full = 0;
}
