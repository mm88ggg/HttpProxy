#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"
#include "cache.h"
#include "proxy.h"

void cache_init(cache_t *cache) {
    // Create the dummy head node.
    cache->head->next = cache->head->prev = cache->head = malloc(sizeof(cache_block));
    cache->size = 0;
    cache->current_count = 0;
    pthread_rwlock_init(&cache->rwlock, NULL);
}

cache_block *find_cache_block(cache_t *cache, char *host, char *port, char *path, cache_policy policy) {
    // Acquire the read lock for the whole cache.
    pthread_rwlock_rdlock(&cache->rwlock);
    cache_block *block = cache->head->next;
    while (block != cache->head) {
        // Check if the block is the one we want.
        if (strcmp(block->host, host) == 0 && strcmp(block->port, port) == 0 && strcmp(block->path, path) == 0) {
            // If the block is found, we need to update the count.
            // Acquire the write lock for the block.
            pthread_rwlock_wrlock(&block->rwlock);
            // Update the count with the policy.
            if (policy == lru) {
                // The reason why I didn't insert the block to the head of the list is that
                // we need to make it be parallel with the read operations.
                // If I insert the block to the head of the list, I need to acquire the write lock
                // that will block the read operations.
                block->count = cache->current_count++;
            } else if (policy == lfu) {
                block->lfu_count++;
            } else {
                fprintf(stderr, "Unknown cache policy\n");
            }
            // Release the write lock for the block.
            pthread_rwlock_unlock(&block->rwlock);
            // Release the read lock for the cache.
            pthread_rwlock_unlock(&cache->rwlock);
            return block;
        }
        block = block->next;
    }
    pthread_rwlock_unlock(&cache->rwlock);
    return NULL;
}

void add_cache_block(cache_t *cache, char *host, char *port, char *path, char *content, size_t size, cache_policy policy) {
    // Create a new block.
    cache_block *block = Malloc(sizeof(cache_block) + size);
    strcpy(block->host, host);
    strcpy(block->port, port);
    strcpy(block->path, path);
    memcpy(block->content, content, size);
    block->size = size;
    block->lfu_count = 0;
    pthread_rwlock_init(&block->rwlock, NULL);

    // Lock the cache.
    pthread_rwlock_wrlock(&cache->rwlock);
    // Find the least recently used or the least frequently used block and delete it.
    while (cache->size + size > MAX_CACHE_SIZE + THREAD_NUM * MAX_OBJECT_SIZE) {
        cache_block *lru_block = cache->head->prev;
        cache_block *block = lru_block->prev;
        while (block != cache->head) {
            if (policy == lru) {
                if (block->count < lru_block->count) {
                    lru_block = block;
                }
            } else if (policy == lfu) {
                if (block->lfu_count < lru_block->lfu_count || (block->lfu_count == lru_block->lfu_count && block->count < lru_block->count)) {
                    lru_block = block;
                }
            } else {
                fprintf(stderr, "Unknown cache policy\n");
            }
            block = block->prev;
        }
        delete_cache_block(cache, lru_block);
    }
    // Add the new block to the head of the list.
    block->count = cache->current_count++;
    block->prev = cache->head;
    block->next = cache->head->next;
    cache->head->next->prev = block;
    cache->head->next = block;
    cache->size += size;
    // Unlock the cache.
    pthread_rwlock_unlock(&cache->rwlock);
}

void delete_cache_block(cache_t *cache, cache_block *block) {
    block->prev->next = block->next;
    block->next->prev = block->prev;
    cache->size -= block->size;
    pthread_rwlock_destroy(&block->rwlock);
    Free(block);
}

void cache_free(cache_t *cache) {
    cache_block *block = cache->head->next;
    while (block != cache->head) {
        cache_block *next = block->next;
        delete_cache_block(cache, block);
        block = next;
    }
    Free(cache->head);
    pthread_rwlock_destroy(&cache->rwlock);
}
