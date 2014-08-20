/**
 * csim.c -- Solution to the Part A of 15213-Cachelab.
 *
 * Name: *****
 * Andrew ID: *****
 *
 * This program receives the parameter (s, E, b) of a cache, creates a cache
 * simulator, and simulates cache hitting, missing, and evicting behavior
 * according to a *valgrind* trace file.
 *
 */
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

/**
 * Cache block. (B = 1 << b) bytes of memory.
 */
typedef struct {
    /* According to the assumption, we ignore the block. char block[] */
} block_t;

/**
 * Cache line, which contains a valid bit, a tag, an access delay recorder and
 * a cache block. The access delay recorder helps to determine which line 
 * should be evicted.
 */
typedef struct {
    int valid;
    int tag;
    int access_delay;
    block_t block;
} line_t;

/**
 * Cache set, which contains line_num numbers of lines.
 */
typedef struct {
    int line_num;
    line_t *lines;
} set_t;

/**
 * The cache simulator, which contains set_num numbers of sets.
 */
typedef struct {
    int set_num;
    set_t *sets;
} cache_t;

/**
 * Simulation result: hit, miss and eviction numbers.
 */
typedef struct {
    int hit_count;
    int miss_count;
    int eviction_count;
} trace_result;

/**
 * The status of one data access. It should be a HIT, a MISS or a EVICTION.
 * Notice that when an EVICTION happens, a MISS must happen too.
 */
typedef enum {
    HIT, MISS, EVICTION
} status_t;

void usage();
cache_t *init_cache(int s, int E);
void destroy_cache(cache_t *cache);
int get_set_index(unsigned long address, int s, int b);
int get_tag(unsigned long address, int s, int b);
void update_lru_status(set_t *set, int selected_line_index);
status_t access_memory(cache_t *cache, unsigned long address, int s, int b);
void trace(cache_t *cache, 
           const char *file_name,
           trace_result *result,
           int s,
           int b);

int main(int argc, char *argv[])
{
    int s = 0, E = 0, b = 0;
    const char *file_name = NULL;
    char *endptr = NULL;  // the endptr parameter of strtol()

    // Parse the command line option.
    char option;
    while((option = getopt(argc, argv, "hs:E:b:t:")) != -1) {
        switch (option) {
            case 'h':
                usage();
                break;
            case 's':
                s = strtol(optarg, &endptr, 10);
                if (s == 0 || (*endptr != '\0')) {
                    /**
                     * optarg should be a string contains only 0-9 and should
                     * not be 0.
                     */
                    usage();
                }
                break;
            case 'E':
                E = strtol(optarg, &endptr, 10);
                if (E == 0 || (*endptr != '\0')) {
                    usage();
                }
                break;
            case 'b':
                b = strtol(optarg, &endptr, 10);
                if (b == 0 || (*endptr != '\0')) {
                    usage();
                }
                break;
            case 't':
                file_name = optarg;
                break;
            default:
                fprintf(stderr, 
                        "%s: invalid option -- '%c'\n", 
                        argv[0], (char) optopt);
                usage();
        }
    }
    if (s == 0 || E == 0 || b == 0) {
        fprintf(stderr, "Missing option (s, E, b), try again!\n");
        exit(EXIT_FAILURE);
    }

    trace_result result = {0, 0, 0};
    cache_t *cache = init_cache(s, E);
    trace(cache, file_name, &result, s, b); // tracing the trace_file
    destroy_cache(cache);

    printSummary(result.hit_count, result.miss_count, result.eviction_count);
    return 0;
}

/**
 * The usage function.
 */
void usage()
{
    fprintf(stderr, 
            "Usage: ./csim [-h] -s <num> -E <num> -b <num> -t <file>\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h         Print this help message.\n");
    fprintf(stderr, "  -s <num>   Number of set index bits.\n");
    fprintf(stderr, "  -E <num>   Number of lines per set.\n");
    fprintf(stderr, "  -b <num>   Number of block offset bits.\n");
    fprintf(stderr, "  -t <file>  Trace file.\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, 
            "  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    exit(EXIT_FAILURE);
}

/**
 * This function initializes the cache, set all cache lines invalid.
 * param: s - number of bits to represent the set index
 *        E - number of lines in a set
 * return: the pointer to a new cache simulator
 */
cache_t *init_cache(int s, int E)
{
    cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
    int set_num = 1 << s;
    int line_num = E;
    cache->set_num = set_num;
    cache->sets = (set_t *)malloc(set_num * sizeof(set_t));

    for (int i = 0; i < set_num; ++i) {
        // Initialize each set.
        cache->sets[i].line_num = line_num;
        cache->sets[i].lines = (line_t *)malloc(line_num * sizeof(line_t));
        for (int j = 0; j < line_num; ++j) {
            // Initialize each line.
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].access_delay = 0;
        }
    }
    return cache;
}

/**
 * This function frees the memory space of a cache simulator.
 * param: cache - pointer to a cache simulator
 */
void destroy_cache(cache_t *cache)
{
    int set_num = cache->set_num;
    for (int i = 0; i < set_num; ++i) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/** 
 * This function extracts the set index from a memory address. It is short, so
 * I define it as an inline function.
 * param: address - the memory address
 * return: set index
 */
inline int get_set_index(unsigned long address, int s, int b)
{
    return (address >> b) & ((1 << s) - 1);
}

/** 
 * This function extracts the tag from a memory address. It is short, so I
 * define it as an inline function.
 * param: address - the memory address
 * return: set index
 */
inline int get_tag(unsigned long address, int s, int b)
{
    return address >> (s + b);
}

/** 
 * This function increases access delay of all lines in a set by 1 except for 
 * the HIT line. Set access delay to 0 for the HIT line.
 * param: set - pointer to a set
 *        selected_line_index - line index of the HIT line
 */
void update_lru_status(set_t *set, int selected_line_index)
{
    for (int i = 0; i < set->line_num; ++i) {
        if (set->lines[i].valid) {
            ++(set->lines[i].access_delay);
        }
    }
    set->lines[selected_line_index].access_delay = 0;
}

/** 
 * This function simulates one data accessing instruction.
 * param: cache - pointer to a cache simulator
 *        address - target memory success
 * return: data accessing status: HIT, MISSING, or EVICTION
 */
status_t access_memory(cache_t *cache, unsigned long address, int s, int b)
{
    // Get the set index and tag of the address.
    int set_index = get_set_index(address, s, b);
    int tag = get_tag(address, s, b);
    set_t *set = &(cache->sets[set_index]);
    // Test whether there is a HIT.
    for (int i = 0; i < set->line_num; ++i) {
        /**
         * If a line is valid and its tag equals the tag of the address, we 
         * get a HIT.
         */
        if ((set->lines[i].valid) && (set->lines[i].tag == tag)) {
            update_lru_status(set, i);
            return HIT;
        }
    }
    // If no HIT returns, we get a MISS.
    for (int i = 0; i < set->line_num; ++i) {
        /**
         * If there exists an invalid line, it is a MISS. We move that memory
         * block into the cache simulator.
         */
        if (!(set->lines[i].valid)) {
            set->lines[i].valid = 1;
            set->lines[i].tag = tag;
            update_lru_status(set, i);
            return MISS;
        }
    }
    /**
     * If no line is invalid, it is an EVICTION. We evict one line according to
     * LRU and move a new line into the cache simulator.
     */
    int eviction_index = 0;
    int longest_delay = set->lines[0].access_delay;
    //Find the index of the line with the longest LRU delay.
    for (int i = 1; i < set->line_num; ++i) {
        if (set->lines[i].access_delay > longest_delay) {
            eviction_index = i;
            longest_delay = set->lines[i].access_delay;
        }
    }
    // eviction
    set->lines[eviction_index].valid = 1;
    set->lines[eviction_index].tag = tag;
    update_lru_status(set, eviction_index);
    return EVICTION;
}

/** 
 * This function traces all data accessing instruction according to the trace 
 * file.
 * param: cache - pointer to a cache simulator
 *        file_name - name of the trace file
 *        result - tracing result
 */
void trace(cache_t *cache, 
           const char *file_name,
           trace_result *result,
           int s,
           int b)
{
    FILE *trace_file = fopen(file_name, "r");
    if (NULL == trace_file) {
        fprintf(stderr, "Error opening %s: %s\n", file_name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    /**
     * For each line of the trace file, we call access_memory() to get the
     * status of that data accessing. Particularly, if it is an EVICTION, it is
     * also a MISS, so we increase both miss_count and eviction_count.
     */
    char trace_line[1024];
    while (NULL != fgets(trace_line, sizeof(trace_line), trace_file)) {
        if ('I' != trace_line[0]) { // memory access
            // Get the address from the trace line.
            unsigned long address;
            sscanf(trace_line + 3, "%lx", &address);
            status_t status = access_memory(cache, address, s, b);
            switch (status) {
                case HIT:
                    ++result->hit_count;
                    break;
                case MISS:
                    ++result->miss_count;
                    break;
                case EVICTION:
                    ++result->miss_count;
                    ++result->eviction_count;
                    break;
            }
            // If the instruction is M, the second memory access must be a HIT.
            if ('M' == trace_line[1]) {
                ++result->hit_count;
            }
        }
    }
}
