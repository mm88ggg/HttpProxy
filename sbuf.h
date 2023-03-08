//
// Created by Xuechao Wang on 10/17/22.
//

#ifndef COMP2310_ASSIGNMENT_2_SBUF_H
#define COMP2310_ASSIGNMENT_2_SBUF_H

#include "csapp.h"

typedef struct sbuf_t {
    int *buf; /* Buffer array */
    int n; /* Maximum number of slots */
    int front; /* buf[(front+1)%n] is first item */
    int rear; /* buf[rear%n] is last item */
//    sem_t mutex; /* Protects accesses to buf */
//    sem_t slots; /* Counts available slots */
//    sem_t items; /* Counts available items */

    // We use pthread_mutex_t and pthread_cond_t instead of sem_t
    pthread_mutex_t mutex;
    pthread_cond_t slots;
    pthread_cond_t items;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

#endif //COMP2310_ASSIGNMENT_2_SBUF_H
