#ifndef COMP2310_CACHE_H
#define COMP2310_CACHE_H

#include <stdlib.h>
#include "csapp.h"

typedef enum {
    lru, lfu
} cache_policy;

typedef struct cache_block {
    char host[MAXLINE];
    char port[6];
    char path[MAXLINE];
    struct cache_block *prev;
    struct cache_block *next;
    int count;
    int lfu_count;
    size_t size;
    pthread_rwlock_t rwlock;
    char content[0];
} cache_block;

typedef struct cache {
    cache_block *head;
    size_t size;
    int current_count;
    pthread_rwlock_t rwlock;
} cache_t;

void cache_init(cache_t *cache);
cache_block *find_cache_block(cache_t *cache, char *host, char *port, char *path, cache_policy policy);
void add_cache_block(cache_t *cache, char *host, char *port, char *path, char *content, size_t size, cache_policy policy);
void delete_cache_block(cache_t *cache, cache_block *block);
void cache_free(cache_t *cache);

#endif
