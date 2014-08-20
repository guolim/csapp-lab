/**
 * cache.h - Header for cache, including cache constant, cache type, 
 * and interfaces
 */
#ifndef CACHE_H
#define CACHE_H

#include "csapp.h"

/* Recommended max cache and object sizes */
extern const size_t MAX_CACHE_SIZE;
extern const size_t MAX_OBJECT_SIZE;

typedef struct cache_node {
    char *tag;
    char *content;
    size_t content_size;
    struct cache_node *prev;
    struct cache_node *next;
} cache_node_t;

typedef struct cache {
    cache_node_t *head;
    cache_node_t *tail;
    size_t total_size;
    size_t total_number;
    pthread_rwlock_t rw_lock;
} cache_t;

/* Function prototypes */
cache_t *init_cache();
int get_cached_object(cache_t *cache,
                      const char *tag,
                      char *content,
                      size_t *size);
void cache_insert(cache_t *cache,
                  const char *tag,
                  const char *content,
                  size_t size);

#endif 
