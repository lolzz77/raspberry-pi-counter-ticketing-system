#include <lgpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "tm1637.h"

// I dk what's this for
#define LFLAGS 0

// GPIO
#define LED 23
#define SWITCH 15

// For Digital Clock
#define CLOCK 20
#define DIO 21

#define QUEUE_SIZE 10

// Code copied from online, not sure `cond` is for what, so are `front, rear` etc
typedef struct {
    int data[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Queue;

// Code copied from online
void initializeQueue(Queue *queue) {
    queue->front = 0;
    queue->rear = -1;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Code copied from online
void enqueue(Queue *queue, int value) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->count >= QUEUE_SIZE) {
        printf("Queue is full. Cannot enqueue.\n");
        pthread_mutex_unlock(&queue->mutex);
        return;
    }
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    queue->data[queue->rear] = value;
    queue->count++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

// Code copied from online
int dequeue(Queue *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    int value = queue->data[queue->front];
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    // dont decrement count here, decrease it somewhere else
    // queue->count--;
    pthread_mutex_unlock(&queue->mutex);
    return value;
}

Queue messageQueue;

void* handleRequest(void* arg) {
    int value;
    int count;
    int h = *(int*)arg;  // Extract the value from the argument
    char szTemp[8];
    do {
        value = dequeue(&messageQueue);
        if(value)
        {
            lgGpioWrite(h, LED, 1); // On LED
            sleep(5);
            messageQueue.count--; // Since u dequeued, decrement count
            count = messageQueue.count;
            // Update digital clock dislpay
            sprintf(szTemp, "%02d %02d", 0, count);
            tm1637ShowDigits(szTemp);
            lgGpioWrite(h, LED, 0); // Off LED
        }
    } while (1);
    return NULL;
}

// call back function
void cbf(int e, lgGpioAlert_p evt, void *data)
{
    char szTemp[8];
    int count;
    // this function will run forever in loop
    // if button is pressed, `e` will be 1, else, will be 0
    if (e)
    {
        count = messageQueue.count + 1;
        if (count > QUEUE_SIZE)
            return;
        // update the digital clock display
        sprintf(szTemp, "%02d %02d", 0, count);
        tm1637ShowDigits(szTemp);

        enqueue(&messageQueue, 1);
    }
}

int main()
{
    int h;
    pthread_t thread;
    char szTemp[8];

    initializeQueue(&messageQueue);

    // Digital clock setup
    tm1637Init(CLOCK, DIO); // clock, data pins
    tm1637SetBrightness(4);
    // Display 0000 numbers on digital clock
    sprintf(szTemp, "%02d %02d", 0, 0);
    tm1637ShowDigits(szTemp);

    h = lgGpiochipOpen(0);
    pthread_create(&thread, NULL, handleRequest, (void*)&h);

    // Setup LED, set GPIO to Output
    lgGpioClaimOutput(h,LFLAGS, LED, 0);
    // Setup button alert, use Rising Edge
    lgGpioClaimAlert(h, 0, LG_RISING_EDGE, SWITCH, 0);
    // For handling button interrupt, function will be called in a loop, endlessly
    lgGpioSetSamplesFunc(cbf, NULL);

    // sleep until interrupted
    while (1) 
        lguSleep(1); 

    lgGpiochipClose(0);
    return 0;
}





    


    