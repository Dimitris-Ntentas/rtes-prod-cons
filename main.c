#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define QUEUESIZE 10
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 5
#define NUM_TASKS 100 // tasks each producer is going to run

typedef struct {
  void *(*work)(void *);
  void *arg;
  void *id;
  struct timeval time_enqueued;
} workFunction;

typedef struct {
  workFunction buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  int done;
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

  FILE *fp = fopen("results.csv", "w"); // Open the file to write headers
  if (fp == NULL) {
    perror("Failed to open file");
    return 1;
  }

  fprintf(fp, "Task Number,Producer ID,Delay (microseconds)\n");
  fclose(fp); 

  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pthread_create(&pro[i], NULL, producer, fifo);
  }
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    pthread_create(&con[i], NULL, consumer, fifo);
  }

  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pthread_join(pro[i], NULL);
  }

  // After all producers are done, indicate that production is finished
  pthread_mutex_lock(fifo->mut);
  fifo->done = 1;
  pthread_cond_broadcast(fifo->notEmpty); // Wake up all waiting consumers
  pthread_mutex_unlock(fifo->mut);

  for (int i = 0; i < NUM_CONSUMERS; i++) {
    pthread_join(con[i], NULL);
  }

  printf("All tasks completed\n");
  queueDelete(fifo);

  return 0;
}

void *producer(void *q) {
  queue *fifo = (queue *)q;
  workFunction wfn;
  int id = rand() % 100 + 1;

  for (int i = 0; i < NUM_TASKS; i++) {
    wfn.work = taskFunction;
    wfn.arg = malloc(sizeof(int));  // Example task: calculating sine of an angle
    *(int *)(wfn.arg) = i;         // Assign task an angle or similar simple value
    wfn.id = malloc(sizeof(int));
    *(int *)(wfn.id) = id;        // Assign the origin id (of the producer) to the process
    

    pthread_mutex_lock(fifo->mut);
    while (fifo->full) {
      printf("Waiting...\n");
      pthread_cond_wait(fifo->notFull, fifo->mut);
    }
    gettimeofday(&wfn.time_enqueued, NULL);
    printf("Producer with id: %d produced task %d.\n", id, i);

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
    FILE *fp = fopen("results.csv", "a");
    while (fifo->empty && !fifo->done) {
      pthread_cond_wait(fifo->notEmpty, fifo->mut);
    }
    if (fifo->empty && fifo->done) {
        pthread_mutex_unlock(fifo->mut);
        break;
    }
    queueDel(fifo, &wfn);
    pthread_cond_signal(fifo->notFull);
    pthread_mutex_unlock(fifo->mut);

    gettimeofday(&now, NULL);
    delay = (now.tv_sec - wfn.time_enqueued.tv_sec) * 1000000L + (now.tv_usec - wfn.time_enqueued.tv_usec);
    fprintf(fp, "%d,%d,%ld\n", *(int *)(wfn.arg), *(int *)(wfn.id), delay);
    fclose(fp);
    printf("Consumed task %d from %d with delay: %ld microseconds.\n",*(int *)(wfn.arg), *(int *)(wfn.id), delay);

    free(wfn.id);
    wfn.work(wfn.arg);
    free(wfn.arg);
  }

  return NULL;
}

void *taskFunction(void *arg) {
  int base_angle = *(int *)arg;
  double radians, sine_val;

//   printf("Calculating sine for 10 angles starting from %d degrees (task %d).\n", base_angle, base_angle);
  for (int i = 0; i < 10; i++) {
    radians = (base_angle + i * 10) * M_PI / 180.0; // Convert degrees to radians
    sine_val = sin(radians); // Calculate sine
    // printf("sin(%d°) = %.2f\n", base_angle + i * 10, sine_val);
  }

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
  q->done = 0;
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
