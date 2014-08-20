/*
 * mm.c
 * Guoli Ma - guolim@shark.ics.cs.cmu.edu
 *
 * Introduction:
 * ============
 * This solution combines segregated free lists with a Binary-Search-Tree to
 * increase both space utilization and throughput. We could use only BST to
 * manage free blocks, but from the block layout (listed below) we can see that
 * small free blocks cannot hold all meta-data for a BST. So I use segregated
 * list to manage small free blocks. Each free list holds free blocks with
 * exactly the same size. For example, if the head of one list is a 24-byte free
 * block, then all of the following free blocks in this list are 24 bytes long.
 *
 * In addition, to maximize space utilization, I omit the footer tag of
 * allocated blocks. Because we use the footer tag only to help locate adjacent
 * free blocks, this change is perfectly OK. But when we want to coalesce free
 * blocks, we have to know whether adjacent blocks are free or allocated. So we
 * add a *prev_alloc* field in the header tag to determine the status of the
 * previous block.
 * 
 * Segregated free list
 * ====================
 * As mentioned above, each list holds free blocks with same size. A example is
 * like this:
 * 
 *          block head
 *         +---------+  +---------+  +---------+  +---------+
 *  8-byte | block 1 |->| block 2 |->| block 3 |->| block 4 |-> ... 
 *         +---------+  +---------+  +---------+  +---------+
 * 16-byte | block 2 |->NULL
 *         +---------+  +---------+  +---------+   
 * 24-byte | block 3 |->| block 1 |->| block 2 |-> ...
 *         +---------+  +---------+  +---------+  
 * 32-byte |  ....   |
 *         +---------+
 *
 * Binary Search Tree
 * ==================
 * Binary Search Tree is used to manage large free blocks. Since all free lists
 * hold free blocks with same size, each node in BST is only the head of a free
 * list. The searching strategy is best-fit, which selects a free block with
 * minimum size among all blocks larger than request. A BST example is like
 * this:
 * 
 *                      Root -> +---------+  +---------+  
 *                              | block 1 |->| block 2 |->...
 *                              +---------+  +---------+  
 *                             / 2048-byte   \
 *                            /              \
 *                           /                \
 *   +---------+  +---------+                  +---------+  +---------+     
 *   | block 2 |<-| block 1 |                  | block 1 |->| block 2 |->...
 *   +---------+  +---------+                  +---------+  +---------+     
 *                 1024-byte                    / 4096-byte
 *                                             /
 *                                      +---------+  +---------+  +---------+        
 *                                      | block 1 |->| block 2 |->| block 3 |
 *                                      +---------+  +---------+  +---------+        
 *                                       3072-byte
 *
 * Block layout
 * ============
 * P = 1 means previous block is allocated, 0 means previous block is free.
 * 
 * A 4-byte pointer means a 4 byte offset between a 8 byte pointer and the base
 * of the heap. According to writeup, the size of the heap is only 2^32 byte
 * long. So we can use a 4 byte offset to represent any byte in the heap.
 * 
 * Allocated block:
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                   size of this block                      |P|1|header
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * bp -> |                                                               |
 *       |                                                               |
 *       |                           payload                             |
 *       |                                                               |
 *       |                                                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       
 * Small free block:
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                   size of this block                      |P|0|header
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * bp -> |             4-byte pointer to next block in list              |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |            4-byte pointer to previous block in list           |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                          ......                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                    size of this block                       |0|footer
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       
 * Large free block:
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                   size of this block                      |P|0|header
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * bp -> |             4-byte pointer to next block in list              |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |            4-byte pointer to previous block in list           |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                                                               |BST
 *       +                     Pointer to left child                     +
 *       |                                                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                                                               |
 *       +                     Pointer to right child                    +
 *       |                                                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                                                               |
 *       +                     Pointer to parent                         +
 *       |                                                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                          ......                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                    size of this block                       |0|footer
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * As we can see, meta-data for BST managed free block is 40 bytes long. So we
 * set THRESHOLD = 40 bytes to distinguish small blocks and large blocks. When
 * block_size <= THRESHOLD, that block is a small block.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "contracts.h"

#include "mm.h"
#include "memlib.h"


// Create aliases for driver tests
// DO NOT CHANGE THE FOLLOWING!
#ifdef DRIVER
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif

/*
 *  Logging Functions
 *  -----------------
 *  - dbg_printf acts like printf, but will not be run in a release build.
 *  - checkheap acts like mm_checkheap, but prints the line it failed on and
 *    exits if it fails.
 */

#ifndef NDEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define checkheap(verbose) do {if (mm_checkheap(verbose)) {  \
                             printf("Checkheap failed on line %d\n", __LINE__);\
                             exit(-1);  \
                        }}while(0)
#else
#define dbg_printf(...)
#define checkheap(...)
#endif

/**
 * Basic constant
 * --------------
 */

static const size_t HWSIZE = 4;          /* Half word and header size (bytes) */
static const size_t WSIZE = 8;           /* Word size of a 64-bit machine */
static const size_t DSIZE = 16;          /* Double word size */
static const size_t CHUNK_SIZE = 1 << 6; /* Extend heap by this amount */
static const size_t MIN_SIZE = 16;       /* Minimum block size */
static const size_t BIN_SIZE = 5;        /* Number of segregated list bins */
static const size_t THRESHOLD = 40;      /* Threshold between seg-list and BST */
static const size_t FREE = 0;            /* Alloc bit */
static const size_t ALLOC = 1;           /* Alloc bit */

/**
 * Global Variables
 * ----------------
 */

static char *heap_listp = NULL;      /* Prologue block */
static uintptr_t heap_base = 0;      /* Start of a heap */
static void *root = NULL;            /* Root of BST for large free blocks */
static uint32_t *bins_offset = NULL; /* Array of heads of free lists */

/*
 *  Helper functions
 *  ----------------
 */

// Align p to a multiple of w bytes
static inline void* align(const void * const p, unsigned char w) {
    return (void*)(((uintptr_t)(p) + (w-1)) & ~(w-1));
}

// Check if the given pointer is 8-byte aligned
static inline int aligned(const void * const p) {
    return align(p, 8) == p;
}

// Return whether the pointer is in the heap.
static int in_heap(const void* p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}


/*
 *  Block Functions
 *  ---------------
 *  The functions below act similar to the macros in the book, but calculate
 *  size in multiples of 4 bytes.
 *  
 *  Since size is calculated in multiples of 4 bytes, the 2 high-order bits
 *  will always be 0. So these block functions use the second high-order bit to
 *  represent the *alloc bit*. Then the block pointer points to the header tag
 *  of the block instead of the first byte of payload. And the size in boundary
 *  tag is the size of payload instead of the size of the whole block.
 *  
 *  I think these functions are less convenient than macro given in text book.
 *  So I rewrite these macros as inline functions and use these functions.
 */

// Return the size of the given block in multiples of the word size
static inline unsigned int block_size(const uint32_t* block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));

    return (block[0] & 0x3FFFFFFF);
}

// Return true if the block is free, false otherwise
static inline int block_free(const uint32_t* block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));

    return !(block[0] & 0x40000000);
}

// Mark the given block as free(1)/alloced(0) by marking the header and footer.
static inline void block_mark(uint32_t* block, int free) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));

    unsigned int next = block_size(block) + 1;
    block[0] = free ? block[0] & (int) 0xBFFFFFFF : block[0] | 0x40000000;
    block[next] = block[0];
}

// Return a pointer to the memory malloc should return
static inline uint32_t* block_mem(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));
    REQUIRES(aligned(block + 1));

    return block + 1;
}

// Return the header to the previous block
static inline uint32_t* block_prev(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));

    return block - block_size(block - 1) - 2;
}

// Return the header to the next block
static inline uint32_t* block_next(uint32_t* const block) {
    REQUIRES(block != NULL);
    REQUIRES(in_heap(block));

    return block + block_size(block) + 2;
}

/****************************
 ** Added helper functions **
 ****************************/
/* Return the larger one of two integers */
static inline int max(int a, int b) {
    return (a > b)? a : b;
}

/* Convert a 8-byte address to a 4-byte offset */
static inline uint32_t addr_to_offset(void *addr) {
    return (addr == NULL)? 0 : (uint32_t)((uintptr_t)addr - heap_base);
}

/* Convert a 4-byte offset to a 8-byte address */
static inline void *offset_to_addr(uint32_t offset) {
    return (offset == 0)? NULL : (void *)(heap_base + offset);
}

/* Pack a size and alloc bit into a 4-bytes value */
static inline uint32_t pack(uint32_t size, uint32_t alloc) {
    /**
     * Since header and footer is 32-bit long, so I use uint32_t instead of
     * size_t.
     */
    REQUIRES(size >= MIN_SIZE);
    REQUIRES(alloc <= 1);

    return size | alloc;
}

/* Read and write half a word at address p */
static inline uint32_t get(void *p) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    return (*(uint32_t *)p);
}
static inline void put(void *p, uint32_t val) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    (*(uint32_t *)p) = val;
}

/* Read the size and allocated fields from address p */
static inline uint32_t get_size(void *p) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    return get(p) & ~0x7;
}
static inline uint32_t get_alloc(void *p) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    return get(p) & 0x1;
}

/* Get and set the prev-alloc fields from address p */
static inline uint32_t get_prev_alloc(void *p) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    return (get(p) & 0x2) >> 1;
}
static inline void set_prev_alloc(void *p, uint32_t prev_alloc) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    *((uint32_t *)p) = (prev_alloc == 1)? (get(p) | 0x2) : (get(p) & (~0x2));
}

/* Given block ptr bp, compute address of its header and footer */
static inline void *hdrp(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    return (char *)bp - HWSIZE;
}
static inline void *ftrp(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    return (char *)bp + get_size(hdrp(bp)) - WSIZE;
}

/* Given block ptr bp, compute address of next and previous blocks */
static inline void *next_blkp(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (char *)bp + get_size((char *)bp - HWSIZE);
}
static inline void *prev_blkp(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (char *)bp - get_size((char *)bp - WSIZE);
}

/* Get predecessor and successor for a free block */
static inline void *prev_free_block(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    uint32_t offset = *((uint32_t *)bp + 1);
    return offset_to_addr(offset);
}
static inline void *next_free_block(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    uint32_t offset = *((uint32_t *)bp);
    return offset_to_addr(offset);
}

/* Set predecessor and successor for a free block */
static inline void set_pred_offset(void *bp, void *pred) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp)); 
    
    *((uint32_t *)bp + 1) = addr_to_offset(pred);
}
static inline void set_succ_offset(void *bp, void *succ) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *((uint32_t *)bp) = addr_to_offset(succ);
}

/* Get left, right child and parent address */
static inline void *get_left_child(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (void *)(*((uintptr_t *)((char *)bp + WSIZE)));
}
static inline void *get_right_child(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (void *)(*((uintptr_t *)((char *)bp + DSIZE)));
}
static inline void* get_parent(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (void *)(*(uintptr_t *)((char *)bp + WSIZE + DSIZE));
}

/* Set left, right child and parent address */
static inline void set_left_child(void *bp, void *left) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *(uintptr_t *)((char *)bp + WSIZE) = (uintptr_t)left;
}
static inline void set_right_child(void *bp, void *right) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *(uintptr_t *)((char *)bp + DSIZE) = (uintptr_t)right;
}
static inline void set_parent(void *bp, void *parent) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *(uintptr_t *)((char *)bp + WSIZE + DSIZE) = (uintptr_t)parent;
}

/**
 * Function prototype
 * ------------------
 */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void replace_child(void *parent, void *cur_chld, void *new_chld);
static void insert_free_block(void *bp);
static void insert_into_seglist(void *bp, size_t size);
static void insert_into_bst(void *bp, size_t size);
static void delete_free_block(void *bp);
static void delete_from_seglist(void *bp, size_t size);
static void delete_from_bst(void *bp);
static void *find_fit(size_t asize);
static void *bst_search(void *node, size_t size);
static void print_block(void *bp);
static int check_block(void *bp);
static void print_free_list(void *node);
static void print_tree(void *node);
static int check_free_list(void *head);
static int check_tree(void *node);

/*
 *  Malloc Implementation
 *  ---------------------
 *  The following functions deal with the user-facing malloc implementation.
 */

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	/* Create the initial empty heap */
	if ((bins_offset = mem_sbrk((BIN_SIZE + 3) * HWSIZE)) == (void *)-1) {
		return -1;
    }
    /* Initialize small free block segregated list bins */
	memset(bins_offset, 0, BIN_SIZE * HWSIZE);
    /* Prologue header pointer */
	heap_listp = (char *)(bins_offset + BIN_SIZE);
    /* BST root pointer */
    root = NULL;
    /* Setup heap base */
    heap_base = (uintptr_t)mem_heap_lo();

	put(heap_listp, pack(WSIZE, ALLOC));                /* Prologue header */
	put(heap_listp + (1 * HWSIZE), pack(WSIZE, ALLOC)); /* Prologue footer */
	put(heap_listp + (2 * HWSIZE), pack(0, ALLOC));		/* Epilogue header */
	heap_listp += HWSIZE;
    set_prev_alloc(hdrp(next_blkp(heap_listp)), ALLOC);
    return 0;
}

/*
 * malloc - Allocate size bytes and returns a pointer to the allocated memory.
 *      If there exists best fit free block, use it. If not, extend the heap 
 *      and use the extended block.
 */
void *malloc(size_t size) {
    checkheap(1);  /* Let's make sure the heap is ok! */

	size_t asize;      /* Adjusted block size */
	size_t extend_size; /* Amount to extend heap if no fit */
	char *bp;

	/* Ignore spurious requests */
	if (size == 0)
		return NULL;


	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= WSIZE) {
		asize = MIN_SIZE;
    } else {
		asize = WSIZE * ((size + (HWSIZE) + (WSIZE - 1)) / WSIZE);
    }

	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL) {
        /* Find fit */
		place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extend_size = max(asize, CHUNK_SIZE);
	if ((bp = extend_heap(extend_size / HWSIZE)) == NULL) {
		return NULL;
    }
	place(bp, asize);
	return bp;
}

/*
 * free - Free the memory space pointed to by ptr. Then insert this block into
 *      segregated list bins or BST
 */
void free(void *ptr) {
    checkheap(1);

    /* Wrong block pointer */
    if (ptr == NULL || !in_heap(ptr) || !aligned(ptr)) {
        return;
    }

    /* Get block size and prev_alloc bit */
	size_t size = get_size(hdrp(ptr));
    size_t prev_alloc = get_prev_alloc(hdrp(ptr));

    /* Reset block to free block */
	put(hdrp(ptr), pack(size, FREE));
	put(ftrp(ptr), pack(size, FREE));
               
    /* Reset prev_alloc field of this block and next block */
    set_prev_alloc(hdrp(ptr), prev_alloc);
    set_prev_alloc(hdrp(next_blkp(ptr)), FREE);

    /* Coalesce if adjacent blocks were free */
	void *new_ptr = coalesce(ptr);
    
    /* Insert new block into segregated list bins or BST */
	insert_free_block(new_ptr);
}

/*
 * realloc - Changes the size of the memory block pointed to by oldptr to size
 *      byte. If the old block is large enough to hold new block, then just
 *      return old block. If the next block is a free block and the combined
 *      size is large enough, then coalesce these two blocks and place a new
 *      allocated block in this new block. If new block is still not large
 *      enough, then just malloc a new block, copy old block into new block and
 *      free old block.
 */
void *realloc(void *oldptr, size_t size) {
	size_t old_size; /* Old block size */
    size_t asize;    /* Adjusted new block size */
	void *new_ptr;   /* New block ptr */

	/* If size == 0 then this is just free, and we return NULL. */
	if (size == 0) {
		free(oldptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if (oldptr == NULL) {
		return malloc(size);
	}

    /* Old block size */
	old_size = get_size(hdrp(oldptr));

    /* Adjust block size to include overhead and alignment reqs */
    if (size < WSIZE) {
        asize = MIN_SIZE;
    } else {
        asize = WSIZE * ((size + (HWSIZE) + (WSIZE-1)) / WSIZE);
    }

    if (asize <= old_size) {
        /* New block can be placed into old blocks */
        return oldptr;
    } else if (!get_alloc(hdrp(next_blkp(oldptr)))) {
        /* Next block is a free block */
        /* Get prev_alloc field of old block and size of new coalesced block */
		size_t new_size = get_size(hdrp(next_blkp(oldptr))) + old_size;
        uint32_t prev_alloc = get_prev_alloc(hdrp(oldptr));
		if (asize < new_size) {
            /* New coalesced block is large enough */
			delete_free_block(next_blkp(oldptr));
			put(hdrp(oldptr), pack(new_size, ALLOC));
			put(ftrp(oldptr), pack(new_size, ALLOC));
            set_prev_alloc(hdrp(oldptr), prev_alloc);
            place(oldptr, asize);
			return oldptr;
		}
	}

    /* Here new block is larger than old block and no free block next to it */
	new_ptr = malloc(size);

	/* If malloc() fails, the original block is left untouched */
	if(!new_ptr) {
		return NULL;
	}

	/* Copy old data into new block, data size is (block size - header size) */
	memcpy(new_ptr, oldptr, old_size - HWSIZE);

	/* Free the old block */
	free(oldptr);

	return new_ptr;
}

/*
 * calloc - Allocate memory for an array of nmemb elements of size bytes each
 *      and returns a pointer to the allocated memory. The memory is set to
 *      zero.
 */
void *calloc (size_t nmemb, size_t size) {
	size_t bytes = nmemb * size;
	void *new_ptr;

	new_ptr = malloc(bytes);
	memset(new_ptr, 0, bytes);

	return new_ptr;
}

/* Returns 0 if no errors were found, otherwise returns the error */
int mm_checkheap(int verbose) {
    void *bp = heap_listp;  

    if (verbose) {
        printf("Segregated list bins:\n");
        for (size_t i = 0; i < BIN_SIZE; ++i) {
            printf("\t0x%08x\n", bins_offset[i]);
        }
    }

    /* Check if prologue header wrong */
    if ((get_size(hdrp(heap_listp)) != WSIZE) || 
                (get_alloc(hdrp(heap_listp)) != ALLOC) ||
                check_block(heap_listp) == 0) {
        printf("\n(ERROR) Prologue header wrong\n");  
        return 0;
    }

    /* Check each block */
    for (bp = heap_listp; get_size(hdrp(bp)) > 0; bp = next_blkp(bp)) {  
        if (verbose) {
            print_block(bp);  
        }
        if (check_block(bp) == 0) {
            printf("\n(ERROR) block %p wrong\n", bp);
            return 0;
        }
    }  

    /* Check Epilogue */
    if (verbose) {  
        print_block(bp);  
    }  
    if ((get_size(hdrp(bp)) != 0) || (get_alloc(hdrp(bp)) != ALLOC)) {
        printf("\n(ERROR) Epilogue header wrong\n"); 
        return 0;
    }

    /* Check segregated free list bins */
    for (size_t i = 0; i < BIN_SIZE; i++) {
        if (bins_offset[i] != 0) {  
            if (verbose) {
                printf("\nSeg-list bin #%lu: size = %lu ", i, (i + 1) * WSIZE);  
                print_free_list(offset_to_addr(bins_offset[i]));  
            }
            if (check_free_list(offset_to_addr(bins_offset[i])) == 0) {
                return 0;
            }
        }  
    }
    if (verbose) {
        printf("\nBinary Search Tree:\n");
        print_tree(root);  
    }
    check_tree(root);  
    printf("cheakheap end\n\n");
    return 1;
}

/**
 * coalesce - Coalesce free blocks. Merge adjacent free blocks into large free
 *      blocks
 */
static void *coalesce(void *bp) {
	void *prev = prev_blkp(bp);
    void *next = next_blkp(bp);

	size_t prev_alloc = get_prev_alloc(hdrp(bp));
	size_t next_alloc = get_alloc(hdrp(next));
	size_t size = get_size(hdrp(bp));

	if (prev_alloc && next_alloc) {
        /* The previous and next blocks are both allocated */
        set_prev_alloc(hdrp(next), FREE);
	} else if (prev_alloc && !next_alloc) {
        /* The previous block is allocated and the next block is free */
		delete_free_block(next);
		size += get_size(hdrp(next));
		put(hdrp(bp), pack(size, FREE));
		put(ftrp(bp), pack(size, FREE));
        set_prev_alloc(hdrp(bp), ALLOC);
	} else if (!prev_alloc && next_alloc) {
        /* The previous block is free and the next block is allocated */
		delete_free_block(prev);
		size += get_size(hdrp(prev));
		put(ftrp(bp), pack(size, FREE));
		put(hdrp(prev), pack(size, FREE));
		bp = prev;
        set_prev_alloc(hdrp(bp), ALLOC);
	} else {
        /* The previous and next blocks are both free */
		delete_free_block(next);
		delete_free_block(prev);
		size += get_size(hdrp(prev)) + get_size(ftrp(next));
		put(hdrp(prev), pack(size, FREE));
		put(ftrp(next), pack(size, FREE));
		bp = prev;
        set_prev_alloc(hdrp(bp), ALLOC);
	}
	return bp;
}

/**
 * extend_heap - Extend heap using mem_sbrk and return the block pointer
 */
static void *extend_heap(size_t hwords) {
 	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (hwords % 2) ? (hwords + 1) * HWSIZE : hwords * HWSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1) {
		return NULL;
    }

	/* Initialize free block header/footer and the epilogue header */
    size_t prev_alloc = get_prev_alloc(hdrp(bp));
	put(hdrp(bp), pack(size, FREE));            /* Free block header */
	put(ftrp(bp), pack(size, FREE));            /* Free block footer */
    set_prev_alloc(hdrp(bp), prev_alloc);
	put(hdrp(next_blkp(bp)), pack(0, ALLOC));	/* New epilogue header */

	/* Coalesce if the previous block was free */
    void *new_ptr = coalesce(bp);
    insert_free_block(new_ptr);
	return new_ptr; 
}

/**
 * place - Given a free block bp and an adjusted malloc size asize, place the
 *      block at the start of bp and split bp if needed
 */
static void place(void *bp, size_t asize) {
	size_t free_size = get_size(hdrp(bp));
    size_t remaining_size = free_size - asize;
    size_t is_realloc = get_alloc(hdrp(bp));
    uint32_t prev_alloc = get_prev_alloc(hdrp(bp));

    /**
     * If block bp is allocated block, then place is called by replace. 
     * So no need to delete bp.
     */
    if (!is_realloc) {
        delete_free_block(bp);
    }

	if (remaining_size >= MIN_SIZE) {
        /* Splitting */
		put(hdrp(bp), pack(asize, ALLOC));
		put(ftrp(bp), pack(asize, ALLOC));
        set_prev_alloc(hdrp(bp), prev_alloc);
		bp = next_blkp(bp);
		put(hdrp(bp), pack(remaining_size, FREE));
		put(ftrp(bp), pack(remaining_size, FREE));
        set_prev_alloc(hdrp(bp), ALLOC);
		insert_free_block(bp);
	} else {
		put(hdrp(bp), pack(free_size, ALLOC));
		put(ftrp(bp), pack(free_size, ALLOC));
        set_prev_alloc(hdrp(bp), prev_alloc);
        set_prev_alloc(hdrp(next_blkp(bp)), ALLOC);
	}
}

/**
 * replace_child - Replace the cur_chld child of parent to new_chld
 */
static void replace_child(void *parent, void *cur_chld, void *new_chld) {
    if (parent == NULL) {
        root = new_chld;
    } else {
        if (cur_chld == get_left_child(parent)) {
            set_left_child(parent, new_chld);
        } else {
            set_right_child(parent, new_chld);
        }
    }
}

/**
 * insert_free_block - Insert a block into segregated free list or BST 
 */
static void insert_free_block(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    size_t size = get_size(hdrp(bp));

	if (size <= THRESHOLD) { /* Small free blocks */
        insert_into_seglist(bp, size);
	} else { /* Large free block */
        insert_into_bst(bp, size);
    }
}

/**
 * insert_into_seglist - Insert the free block into seglist
 */
static void insert_into_seglist(void *bp, size_t size) {
    size_t index = (size / WSIZE) - 1;
    void *free_listp = offset_to_addr(bins_offset[index]);

    if (free_listp != NULL) { /* The free list has block in it */
        /* Insert bp at the head of the list */
        set_pred_offset(bp, NULL);
        set_succ_offset(bp, free_listp);
        set_pred_offset(free_listp, bp);
        free_listp = bp;
    } else { /* The free list has nothing in it */
        free_listp = bp;
        set_pred_offset(bp, NULL);
        set_succ_offset(bp, NULL);
    }
    bins_offset[index] = addr_to_offset(free_listp);
}

/**
 * insert_into_bst - Insert the free block into BST
 */
static void insert_into_bst(void *bp, size_t size) {
    /* When searching bst, temp will record previous visited node */
    void *temp = NULL; 
    void *curr_node = root;
    void *left_child = NULL;
    void *right_child = NULL;

    /* Clear the free block */
    set_succ_offset(bp, NULL);
    set_pred_offset(bp, NULL);
    set_left_child(bp, NULL);
    set_right_child(bp, NULL);
    set_parent(bp, NULL);

    /* Figure out where to put the new node in BST */
    if (root == NULL) {
        root = bp;
        return;
    }

    while (curr_node) {
        temp = curr_node;
        size_t curr_size = get_size(hdrp(curr_node));

        if (size < curr_size) {
            curr_node = get_left_child(curr_node);
        } else if (size > curr_size) {
            curr_node = get_right_child(curr_node);
        } else {
            /* Same size, insert bp into the list and use bp as the head */

            /* Insert bp into list */
            set_succ_offset(bp, curr_node);
            set_pred_offset(curr_node, bp);

            /* Set bp as a node in BST*/
            left_child = get_left_child(curr_node);
            set_left_child(bp, left_child);
            if (left_child != NULL) {
                set_parent(left_child, bp);
            }
            right_child = get_right_child(curr_node);
            set_right_child(bp, right_child);
            if (right_child != NULL) {
                set_parent(right_child, bp);
            }
            void *parent = get_parent(curr_node);
            set_parent(bp, parent);
            replace_child(parent, curr_node, bp);

            /* Clear tree related field of curr_node */
            set_left_child(curr_node, NULL);
            set_right_child(curr_node, NULL);
            set_parent(curr_node, NULL);
            return;
        }
    }
    /**
     * curr_node becomes NULL, which means no block sized *size* is found.
     * Insert bp into BST as a leaf node
     */
    set_parent(bp, temp);
    if (size < get_size(hdrp(temp))) {
        set_left_child(temp, bp);
    } else {
        set_right_child(temp, bp);
    }
}

/**
 * delete_free_block - Delete free block from the segregated list or BST
 */
static void delete_free_block(void *bp) {
    size_t size = get_size(hdrp(bp));
    if (size <= THRESHOLD) {
        delete_from_seglist(bp, size);
    } else {
        delete_from_bst(bp);
    }
}

/**
 * delete_from_seglist - Delete free block from the segregated list
 */
static void delete_from_seglist(void *bp, size_t size) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    size_t index = (size / WSIZE) - 1;
    void *free_listp = offset_to_addr(bins_offset[index]);
    void *pred = prev_free_block(bp);
    void *succ = next_free_block(bp);
    
    if (pred == NULL) {
        /* The block is the first block of the free list */
        free_listp = succ;
        if (free_listp != NULL) {
            set_pred_offset(free_listp, NULL);
        }
    } else if (succ == NULL) {
        /* The block is the last block of the free list */
        set_succ_offset(pred, NULL);
    } else {
        /* Normal block */
        set_succ_offset(pred, succ);
        set_pred_offset(succ, pred);
    }
    bins_offset[index] = addr_to_offset(free_listp);
}

/**
 * delete_from_bst - Delete free block from the BST
 */
static void delete_from_bst(void *bp) {
    void *pred = prev_free_block(bp);
    void *succ = next_free_block(bp);
    if (pred != NULL) {
        /* bp is not the head of the list */
        set_succ_offset(pred, succ);
        if (succ != NULL) {
            /* bp is not the end of the list */
            set_pred_offset(succ, pred);
        }
    } else {
        /* bp is the head of the list */
        void *left_child = get_left_child(bp);
        void *right_child = get_right_child(bp);
        void *parent = get_parent(bp);

        if (succ != NULL) {
            /**
             * bp has successor, remove bp from the list and let the successor 
             * be the node in the BST
             */
            /* remove bp from the list */
            set_pred_offset(succ, NULL);

            /* successor be the node in BST */
            set_left_child(succ, left_child);
            if (left_child != NULL) {
                set_parent(left_child, succ);
            }

            set_right_child(succ, right_child);
            if (right_child != NULL) {
                set_parent(right_child, succ);
            }

            set_parent(succ, parent);
            replace_child(parent, bp, succ);
        } else {
            /* bp has no successor, single node */
            if (left_child == NULL && right_child == NULL) {
                /* bp is the leaf node */
                if (parent == NULL) {
                    root = NULL;
                } else {
                    replace_child(parent, bp, NULL);
                }
            } else if (left_child == NULL && right_child != NULL) {
                /* No left child, replace bp with its right child */
                set_parent(right_child, parent);
                replace_child(parent, bp, right_child);
            } else if (left_child != NULL && right_child == NULL) {
                /* No right child, replace bp with its left child */
                set_parent(left_child, parent);
                replace_child(parent, bp, left_child);
            } else {
                /* bp has left and right child */
                if (get_left_child(right_child) == NULL) {
                    /**
                     * bp's right child does not have left child, let bp's left
                     * child be the left child of bp's right child and replace
                     * bp with bp's right child
                     */
                    set_left_child(right_child, left_child);
                    set_parent(left_child, right_child);
                    set_parent(right_child, parent);
                    replace_child(parent, bp, right_child);
                } else {
                    /**
                     * bp's right child has left child, find the node with
                     * minimum size in right tree. Replace bp with this minimum
                     * sized node.
                     */
                    void *left_min_child = right_child;
                    void *temp = left_min_child;
                    /* Find node with minimum size in right tree */
                    while (get_left_child(left_min_child)) {
                        temp = left_min_child;
                        left_min_child = get_left_child(left_min_child);
                    }
                    /* Replace left_min_child with its right child */
                    set_left_child(temp, get_right_child(left_min_child));
                    if (get_right_child(left_min_child) != NULL) {
                        set_parent(get_right_child(left_min_child), temp);
                    }
                    /* Replace bp with left_min_child */
                    set_left_child(left_min_child, left_child);
                    set_right_child(left_min_child, right_child);
                    set_parent(left_min_child, parent);
                    set_parent(left_child, left_min_child);
                    set_parent(right_child, left_min_child);
                    replace_child(parent, bp, left_min_child);
                }
            }
        }
    }
}

/**
 * find_fit - Find a free block with asize bytes
 */
static void *find_fit(size_t asize) {
	if (asize < THRESHOLD) {
        size_t index = asize / WSIZE - 1;
        void *free_listp = offset_to_addr(bins_offset[index]);
		if (free_listp != NULL) {
			/* Found a free list with asize bytes */
			return free_listp;
		}
	}

    /* No seg-list block found or asize fits in BST */
    return bst_search(root, asize);
}

/**
 * bst_search - Search the best fit node
 */
static void *bst_search(void *node, size_t size) {
    /* No BST */
    if (node == NULL) {
        return NULL;
    }

    void *fit = NULL;
    size_t node_size = get_size(hdrp(node));

    /* Perfectly match */
    if (size == node_size) {
        return node;
    } else if (size < node_size) {
        /* Search left child */
        fit = bst_search(get_left_child(node), size);
        if (fit != NULL) { /* If find best fit in left child */
            return fit;
        } else { /* No fit found, then node is best fit */
            return node;
        }
    } else {
        return bst_search(get_right_child(node), size);
    }
}

/**
 * print_block - Print all meta-data of a block
 */
static void print_block(void *bp) {  
    uint32_t head_size = get_size(hdrp(bp));  
    uint32_t head_alloc = get_alloc(hdrp(bp));  
    uint32_t foot_size = get_size(ftrp(bp));  
    uint32_t foot_alloc = get_alloc(ftrp(bp));  
    uint32_t prev_alloc = get_prev_alloc(hdrp(bp));
  
    if (head_size == 0)  {  
        printf("%p: Epilogue block\n", bp);  
        return;  
    }  

    printf("%p: header [%u|%u|%u]\n", bp, head_size, prev_alloc, head_alloc);

    if (head_alloc) { /* Allocated block */
        printf("%p: NO footer\n", bp);
    } else { /* Free block */
        printf("%p: footer [%u|%u]\n", bp, foot_size, foot_alloc);
        printf("\tIn list, PREV = %p, NEXT = %p\n", 
               prev_free_block(bp),
               next_free_block(bp));
        /* Large free block */
        if (get_size(hdrp(bp)) > THRESHOLD) {
            if (bp == root || get_parent(bp) != NULL) {
                printf("\tIn BST, Parent = %p, Left = %p, Right = %p\n",
                        get_parent(bp),
                        get_left_child(bp),
                        get_right_child(bp));
            }
        }
    }  
}  

/* 
 * check_block - Check the consistency of a block
 */  
static int check_block(void *bp) {  
    if (!aligned(bp)) {
        printf("\n(Error) %p is not 8-byte aligned\n", bp);  
        return 0;
    }

    /* Free block, check if header and footer matched */
    if ((get_alloc(hdrp(bp)) == FREE) &&
        (get(hdrp(bp)) & ~0x2) != (get(ftrp(bp)) & ~0x2)) {
        printf("\n(Error) header and footer does not match\n");
        printf("\theader = 0x%08x, footer = 0x%08x\n",
                get(hdrp(bp)),
                get(ftrp(bp)));
        return 0;
    }

    /**
     * Check the prev_alloc field. Since allocated block does not have footer
     * tag, we can only check the prev_alloc of the next block
     */
    uint32_t prev_alloc = get_prev_alloc(hdrp(next_blkp(bp)));
    if (get_alloc(hdrp(bp)) != prev_alloc) {
        printf("\n(Error) %p block's prev_alloc wrong\n", next_blkp(bp));
        printf("        previous block is %s block, prev_alloc = %s\n",
               (get_alloc(bp) == ALLOC)? "ALLOC" : "FREE",
               (prev_alloc == ALLOC)? "ALLOC" : "FREE");
        return 0;
    }
    return 1;
}  

/**
 * print_free_list - Print the entire free list
 */
static void print_free_list(void *node) {  
    printf("\n");
    while (node)  {  
        print_block(node);  
        printf("==>\n");  
        node = next_free_block(node);  
    }  
}  
  
/**
 * print_tree - Print the entire BST
 */
static void print_tree(void *node) {  
    if (node == NULL)  
        return;  
    print_free_list(node);  
    print_tree(get_left_child(node));  
    print_tree(get_right_child(node));  
}  

/**
 * check_free_list - Check the consistency of a free list
 */
static int check_free_list(void *head) {  
    for (void *bp = head; bp != NULL; bp = next_free_block(bp)) {
        /* Check whether PREV pointer wrong */
        if (prev_free_block(bp) && /* Has PREV */
            next_free_block(prev_free_block(bp)) != bp) {
            printf("\n(ERROR) %p block PREV pointer wrong\n", bp);  
            return 0;
        }
    }

    return 1;
}  
  
/**
 * check_tree - Check the consistency of the BST
 */
static int check_tree(void *node) {  
    /* NULL node */
    if (node == NULL) {
        return 1;  
    }

    /* Check whether the parent pointer is wrong */
    if (get_parent(node) != NULL) {
        if (get_left_child(get_parent(node)) != node &&
            get_right_child(get_parent(node)) != node) {
            printf("\n(ERROR) parent pointer of %p wrong\n", node);
            return 0;
        }
    }

    if (check_free_list(node) == 0 ||
        check_tree(get_left_child(node)) == 0 || 
        check_tree(get_right_child(node)) == 0) {
        return 0;
    }
    return 1;
}  
