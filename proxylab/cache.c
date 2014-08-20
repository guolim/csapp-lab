/**
 * cache.c - A thread safe web content cache, using LRU evction strategy. Cache
 * is implemented through linked list. The head node is a most recent used web
 * object and the tail is the least one.
 */
#include "cache.h"

/* Recommended max cache and object sizes */
const size_t MAX_CACHE_SIZE = 1049000;
const size_t MAX_OBJECT_SIZE = 102400;

/* Helper function prototypes */
static cache_node_t *create_cache_node(const char *tag,
                                       const char *content,
                                       size_t size);
static void destroy_cache_node(cache_node_t *node);
static cache_node_t *search_node(cache_t *cache, const char *tag);
static void insert_to_head(cache_t *cache, cache_node_t *node);
static cache_node_t *delete_from_list(cache_t *cache, cache_node_t *node);

/*******************************
 ** Cache operation interface **
 *******************************/
/**
 * init_cache - Initialize cache list and reader writer lock
 */
cache_t *init_cache()
{
    cache_t *cache = (cache_t *)Malloc(sizeof(cache_t));

    cache->head = NULL;
    cache->tail = NULL;
    cache->total_size = 0;
    cache->total_number = 0;

    int err;
    if ((err = pthread_rwlock_init(&cache->rw_lock, NULL)) != 0) {
        fprintf(stderr, "Cannot init rwlock, err = %d\n", err);
        exit(EXIT_FAILURE);
    }

    return cache;
}

/**
 * get_cached_object - According to the tag, search the entire cache. If hit,
 *      copy the object to the content parameter.
 */
int get_cached_object(cache_t *cache,
                      const char *tag,
                      char *content,
                      size_t *size)
{
    pthread_rwlock_rdlock(&cache->rw_lock);

    int hit = 0;
    cache_node_t *node = search_node(cache, tag);

    if (node != NULL) {
        /* Got hit! */
        hit = 1;
        *size = node->content_size;
        memcpy(content, node->content, node->content_size);
        /* LRU update, most recent used object is at the head of list */
        if (node != cache->head) {
            delete_from_list(cache, node);
            insert_to_head(cache, node);
        }
    }

    pthread_rwlock_unlock(&cache->rw_lock);

    return hit;
}

/**
 * cache_insert - Insert a new object to the cache list.
 */
void cache_insert(cache_t *cache,
                  const char *tag,
                  const char *content,
                  size_t size)
{
    pthread_rwlock_wrlock(&cache->rw_lock);

    if (cache->total_size + size > MAX_CACHE_SIZE) {
        /* No enough cache space, eviction */
        /* Least recent used object is always at the tail of list */
        cache_node_t *old_tail = delete_from_list(cache, cache->tail);
        destroy_cache_node(old_tail);
    }
    cache_node_t *node = create_cache_node(tag, content, size);
    insert_to_head(cache, node);

    pthread_rwlock_unlock(&cache->rw_lock);
}

/***********************************
 ** Cache operation interface end **
 ***********************************/

/*********************
 ** Helper function **
 *********************/
/**
 * create_cache_node - Create a cache list node. Dynamically allocate space for
 *      a new object.
 */
static cache_node_t *create_cache_node(const char *tag,
                                       const char *content,
                                       size_t size)
{
    cache_node_t *node = (cache_node_t *)Malloc(sizeof(cache_node_t));

    node->tag = (char *)Malloc(strlen(tag) + 1);
    strcpy(node->tag, tag);

    node->content = (char *)Malloc(size);
    memcpy(node->content, content, size);

    node->content_size = size;

    node->prev = NULL;
    node->next = NULL;
    
    return node;
}

/**
 * destroy_cache_node - Frees the allocated space of a useless node.
 */
static void destroy_cache_node(cache_node_t *node)
{
    if (node != NULL) {
        Free(node->tag);
        Free(node->content);
        Free(node);
    }
}

/**
 * search_node - Search for a node according to its tag.
 */
static cache_node_t *search_node(cache_t *cache, const char *tag)
{
    for (cache_node_t *node = cache->head;
         node != NULL;
         node = node->next) {
        if (strcmp(node->tag, tag) == 0) {
            return node;
        }
    }
    return NULL;
}

/**
 * insert_to_head - Insert a cache node to the head of the cache list.
 */
static void insert_to_head(cache_t *cache, cache_node_t *node) 
{
    if (cache->total_number == 0) {
        /* Insert to a empty list */
        cache->head = node;
        cache->tail = node;
        node->prev = NULL;
        node->next = NULL;
    } else {
        /* Insert into non-empty list */
        cache->head->prev = node;
        node->next = cache->head;
        node->prev = NULL;
        cache->head = node;
    }
    ++cache->total_number;
    cache->total_size += node->content_size;
}

/**
 * delete_from_list - Delete a node from the cache list.
 */
static cache_node_t *delete_from_list(cache_t *cache, cache_node_t *node)
{
    if (cache->total_number == 0) {
        /* Cache is empty */
        return NULL;
    }
    if (cache->head == cache->tail) {
        /* Cache is empty after deleting */
        cache->head = NULL;
        cache->tail = NULL;
    } else if (node->prev == NULL) {
        /* Delete head node */
        cache->head = node->next;
        cache->head->prev = NULL;
    } else if (node->next == NULL) {
        /* Delete tail node */
        cache->tail = node->prev;
        cache->tail->next = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    --cache->total_number;
    cache->total_size -= node->content_size;
    return node;
}

/*************************
 ** Helper function end **
 *************************/
