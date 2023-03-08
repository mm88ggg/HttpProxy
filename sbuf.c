#include "csapp.h"
#include "sbuf.h"

// Create an empty, bounded, shared FIFO buffer with n slots
void sbuf_init(sbuf_t *sp, int n) {
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n; /* Buffer holds max of n items */
    sp->front = sp->rear = 0; /* Empty buffer iff front == rear */
//    Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
//    Sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
//    Sem_init(&sp->items, 0, 0); /* Initially, buf has zero data items */
    pthread_mutex_init(&sp->mutex, NULL);
    pthread_cond_init(&sp->slots, NULL);
    pthread_cond_init(&sp->items, NULL);
}

// Clean up buffer sp
void sbuf_deinit(sbuf_t *sp) {
    Free(sp->buf);
    pthread_mutex_destroy(&sp->mutex);
    pthread_cond_destroy(&sp->slots);
    pthread_cond_destroy(&sp->items);
}

// Insert item onto the rear of shared buffer sp
// PV version is equivalent to conditional lock version
void sbuf_insert(sbuf_t *sp, int item) {
//    P(&sp->slots); /* Wait for available slot */
//    P(&sp->mutex); /* Lock the buffer */
//    sp->buf[(++sp->rear)%(sp->n)] = item; /* Insert the item */
//    V(&sp->mutex); /* Unlock the buffer */
//    V(&sp->items); /* Announce available item */

    // Wait for available slot
    pthread_mutex_lock(&sp->mutex);
    while ((sp->rear + 1) % sp->n == sp->front) {
        pthread_cond_wait(&sp->slots, &sp->mutex);
    }
    // Insert the item
//    sp->buf[(++sp->rear) % (sp->n)] = item;
    sp->buf[(sp->rear = (sp->rear + 1) % sp->n)] = item;
    // Announce available item
    pthread_cond_signal(&sp->items);
    pthread_mutex_unlock(&sp->mutex);
}

// Remove and return the first item from buffer buf
// PV version is equivalent to conditional lock version
int sbuf_remove(sbuf_t *sp) {
//    int item;
//    P(&sp->items); /* Wait for available item */
//    P(&sp->mutex); /* Lock the buffer */
//    item = sp->buf[(++sp->front)%(sp->n)]; /* Remove the item */
//    V(&sp->mutex); /* Unlock the buffer */
//    V(&sp->slots); /* Announce available slot */
//    return item;

    int item;
    // Wait for available item
    pthread_mutex_lock(&sp->mutex);
    while (sp->rear == sp->front) {
        pthread_cond_wait(&sp->items, &sp->mutex);
    }
    // Remove the item
//    item = sp->buf[(++sp->front) % (sp->n)];
    item = sp->buf[(sp->front = (sp->front + 1) % sp->n)];
    // Announce available slot
    pthread_cond_signal(&sp->slots);
    pthread_mutex_unlock(&sp->mutex);
    return item;
}
