/*
 * mm.c - Dynamic Storage Allocator
 *
 * xke - Xuyan Ke
 *
 * Segregated Free List + BST + Best fit
 * 
 * Block struct:
 * 
 *    Block size | info* | Header(4 bytes)       
 *                ...                          
 *              Payload                        
 *                ...                          
 *                ...                          
 *                ...                          
 *                ...                          
 *            Padding(optional)             
 * 
 *         a) Allocated block               
 *
 *
 *  Block size |info*  | Header(4 bytes)     
 *        succ(Successor) 4 bytes            
 * 
 *      b) Mini free block (8 bytes)
 *
 *
 *  Block size |info*  | Header(4 bytes)     Block size|info* | Header (4 bytes)
 *        succ(Successor) 4 bytes                   succ(Successor) 4 bytes
 *       pred(Predecessor) 4 bytes                 pred(Predecessor) 4 bytes
 *             ...                                      left child
 *             ...                                      right child
 *             ...                                        parent 
 *             ...                                         ...
 *    Block size | Footer (4 bytes)               Block size | Footer (4 bytes)
 * 
 *      b) Small free block                       c) Large Free block
 *
 * Header info: prev_alloc | alloc
 *
 * Note: header, footer, predecessor and successor are optimized 
 *       for this lab only.
 * 
 *
 * Segregated free list + bst struct:
 * 
 * For bins less than and equal to 32 bytes, hold only one size free block.
 * The last bin is used as bst root, hold all blocks larger than 32 bytes.
 * All blocks in the same size are put into a doubly linked list and the 
 * head of list is a node of the bst.
 *
 * Bin 0: Singly linked list for 8 bytes blocks
 * Bin 1-3: Doubly linked list for 16, 24 and 32 bytes blocks respectively
 * Bin 4: BST for blocks size larger than or equal to 40 bytes
 *
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

//Basic constants
#define HWSIZE 4 // Half word and header/footer size (bytes)
#define WSIZE 8  // Word size (bytes)
#define DSIZE 16 // Double word size (bytes)
#define CHUNKSIZE (1<<8) // Extend heap by this amount (bytes)
#define MIN_SIZE 8 // Minimum block size
#define LISTNUM 5 // Number of free lists
#define THRESHOLD 32 // Bins for size less 32 bytes hold one size

#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

static void *heap_base;  // The lowest address of heap
static char *heap_listp; // The first block of heap
static void **free_lists; // The pointer of array of free lists, 
                          // the array will be put in the heap.
                          // Size: 8, 16, 24, 32 and >= 40 (bytes)

// Prototype functions
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free_lists (void *bp);
static void insert_free(void *bp, int index);
static void bst_insert(void *bp);
static void delete_free_lists(void *bp);
static void delete_free(void *bp, int index);
static void bst_delete(void *bp);
static void check_bst(void *node, int count);
static int count_bst(void *node);

/*
 *  Helper functions
 */

// Align p to a multiple of w bytes
static inline void* align(const void const* p, unsigned char w) {
    return (void*)(((uintptr_t)(p) + (w-1)) & ~(w-1));
}

// Check if the given pointer is 8-byte aligned
static inline int aligned(const void const* p) {
    return align(p, 8) == p;
}

// Return whether the pointer is in the heap.
static int in_heap(const void* p) {
    return p <= (void *)((char *)mem_heap_hi() + 1) && p >= mem_heap_lo();
}

/*
 *  Block Functions
 *  ---------------------
 *  The following functions deal with pointer arithmetic for blocks
 */

// Pack a size and status field into a word
static inline uint32_t pack(uint32_t size,
                            uint32_t alloc,
                            uint32_t prev_alloc,
                            uint32_t prev_small) {
    return size | alloc | (prev_alloc << 1) | (prev_small << 2);
}

// Read and write half a word at address *p
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

// Read the size and allocated fields from address p
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

static inline uint32_t get_prev_alloc(void *p) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    return (get(p) & 0x2) >> 1;
}

static inline uint32_t get_prev_small(void *p) {
    REQUIRES(p != NULL);
    REQUIRES(in_heap(p));

    return (get(p) & 0x4) >> 2;
}

// Given block ptr bp, compute address of its header and footer
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

// Given block ptr bp, compute address of next and previous blocks
static inline void *next_blkp(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (char *)bp + get_size((char *)bp - HWSIZE);
}

static inline void *prev_blkp(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    if (get_prev_small(hdrp(bp))) {
	return (char *)bp - MIN_SIZE;
    }
    
    return (char *)bp - get_size((char *)bp - WSIZE);
}

// Get predecessor and successor from a free block
static inline void *get_pred (void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    uint32_t offset = *((uint32_t *)((char *)bp + HWSIZE));

    if (!offset) {
	return NULL;
    }
    return (void *)((size_t)heap_base + offset);
}

static inline void *get_succ(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    uint32_t offset = *((uint32_t *)bp);
    
    if (!offset) {
	return NULL;
    }
    return (void *)((size_t)heap_base + offset);
}

// Set predecessor and successor for a free block
static inline void set_pred(void *bp, void *pred) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp)); 
    
    if (pred) {
	*((uint32_t *)((char *)bp + HWSIZE)) = 
	    (uint32_t)((size_t)pred - (size_t)heap_base);
    }
    else {
	*((uint32_t *)((char *)bp + HWSIZE)) = (uint32_t)0;
    }
}

static inline void set_succ(void *bp, void *succ) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    if (succ) {
	*(uint32_t *)bp = (uint32_t)((size_t)succ - (size_t)heap_base);
    }
    else {
	*(uint32_t *)bp = (uint32_t)0;	
    }
}

// Get left, right child and parent address
static inline void* get_left(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (void *)(*(size_t *)((char *)bp + WSIZE));
}

static inline void* get_right(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (void *)(*(size_t *)((char *)bp + DSIZE));
}

static inline void* get_parent(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    return (void *)(*(size_t *)((char *)bp + WSIZE + DSIZE));
}

// Set left, right child and parent address
static inline void set_left(void *bp, void *left) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *(size_t *)((char *)bp + WSIZE) = (size_t)left;
}

static inline void set_right(void *bp, void *right) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *(size_t *)((char *)bp + DSIZE) = (size_t)right;
}

static inline void set_parent(void *bp, void *parent) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    *(size_t *)((char *)bp + WSIZE + DSIZE) = (size_t)parent;
}

// Gien block size return list index
static inline int get_index(uint32_t size) {
    if (size <= THRESHOLD) {
	return (size - WSIZE) / WSIZE;
    }
    return LISTNUM - 1;
}

// Find minimum and maximum node of a bst
static inline void *tree_minimum(void *node) {
    while(get_left(node)) {
	node = get_left(node);
    }
    return node;
}

static inline void *tree_maximum(void *node) {
    while(get_right(node)) {
	node = get_right(node);
    }
    return node;
}

// Replace child
static inline void replace_child(void *parent, void *cur, void *child) {
    if (parent) {
	if (cur == get_left(parent)) {
	    set_left(parent, child);
	}
	else {
	    set_right(parent, child);
	}
    }
    else {
	free_lists[LISTNUM - 1] = child;
    }
}
/*
 * insert_free_lists - Insert free block into free lists, for block size
 *                     larger than THRESHOLD bytes, insert into bst.
 */
static void insert_free_lists (void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    uint32_t size = get_size(hdrp(bp));
    int index = get_index(size);

    if (index < LISTNUM - 1) {
	// Insert block into segregated list
	insert_free(bp, index);
    }
    else {
	bst_insert(bp);
    }
    
    // Clear prev_alloc for next block's header
    void *next_hdrp = hdrp(next_blkp(bp));
    uint32_t prev_small = 0;
    if (get_size(hdrp(bp)) <= MIN_SIZE) {
	prev_small = 1;
    }
    put(next_hdrp, pack(get_size(next_hdrp), 
			            get_alloc(next_hdrp), 
			            0,
			            prev_small));
}

/*
 * insert_free - Insert a small free block into a free list
 *                     
 */
static void insert_free (void *bp, int index) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
    
    if (index > 0) {
	char *free_listp = free_lists[index];
	set_pred(bp, NULL);
	set_succ(bp, NULL);
	if (free_listp) {
	    void *first = free_listp;
	    free_listp = bp;
	    set_succ(bp, first);
	    set_pred(first, bp);
	}
	else {
	    free_listp = bp;
	}

	free_lists[index] = free_listp;
    }
    else {
	char *head = free_lists[index];
	set_succ(bp, head);
	free_lists[index] = bp;
    }
}

/*
 * bst_insert - Insert a large free block into bst
 *                     
 */
static void bst_insert(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    void *x = free_lists[LISTNUM - 1];
    void *y = NULL;
    uint32_t size = get_size(hdrp(bp));
    
    set_succ(bp, NULL);
    set_pred(bp, NULL);
    set_parent(bp, NULL);
    set_left(bp, NULL);
    set_right(bp, NULL);

    while (x) {
	// Find a node has the same size or has no corresponding child
	y = x;
	uint32_t cur_size = get_size(hdrp(x));
	if (size == cur_size) {
	    // If the same size, insert block into the list
	    // and use it as head of the list
	    set_succ(bp, x);
	    set_pred(x, bp);
	    set_left(bp, get_left(x));
	    if (get_left(x)) {
		set_parent(get_left(x), bp);
	    }
	    set_right(bp, get_right(x));
	    if (get_right(x)) {
		set_parent(get_right(x), bp);
	    }
	    set_parent(bp, get_parent(x));
	    replace_child(get_parent(x), 
			  x, 
			  bp);
	    set_parent(x, NULL);
	    set_left(x, NULL);
	    set_right(x, NULL);
	    return;
	}
	else if (size < cur_size) {
	    x = get_left(x);
	}
	else {
	    x = get_right(x);
	}
    }
    if (!y) {
	// Set bp as root
	free_lists[LISTNUM - 1] = bp;
    }
    else if (size < get_size(hdrp(y))) {
	set_left(y, bp);
	set_parent(bp, y);
    }
    else {
	set_right(y, bp);
	set_parent(bp, y);
    }
}

/*
 * delete_free_lists - Delete free block from free lists, for large blocks
 *                     delete them from bst.
 */
static void delete_free_lists (void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    uint32_t size = get_size(hdrp(bp));
    int index = get_index(size);
    if (index < LISTNUM - 1) {
	// Delete block into segregated list
	delete_free(bp, index);
    }
    else {
	bst_delete(bp);
    }
    // Set prev_alloc for next block's header
    void *next_hdrp = hdrp(next_blkp(bp));
    uint32_t prev_small = 0;
    if (get_size(hdrp(bp)) <= MIN_SIZE) {
	prev_small = 1;
    }
    put(next_hdrp, pack(get_size(next_hdrp), 
			            get_alloc(next_hdrp), 
			            1,
			            prev_small));
}

/*
 * delete_free - Delete a small free block from a free list
 *                     
 */
static void delete_free (void *bp, int index) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));
   
    if (index > 0) {
	char* free_listp = free_lists[index];
	void *pred = get_pred(bp);
	void *succ = get_succ(bp);

	if (!pred) {
	    // This block is the frist block of free list
	    free_listp = succ;
	}
	else {
	    // If pred is not NULL, set pred
	    set_succ(pred, succ);
	}
	if (succ) {
	    // If succ is not NULL, set succ
	    set_pred(succ, pred);
	}
	set_pred(bp, NULL);
	set_succ(bp, NULL);

	free_lists[index] = free_listp;
    }
    else {
	char *head = free_lists[index];
	char *prev = NULL;
	while (head != bp) {
	    prev = head;
	    head = get_succ(head);
	}
	if (prev == NULL) {
	    free_lists[index] = get_succ(head);
	}
	else {
	    set_succ(prev, get_succ(bp));
	}
    }
}

/*
 * bst_delete - Delete a large free block from bst
 *                     
 */
static void bst_delete(void *bp) {
    REQUIRES(bp != NULL);
    REQUIRES(in_heap(bp));

    if (!get_pred(bp)) {
	// Block is the head of the node
	void *next = get_succ(bp);
	if (next) {
	    // Has next, delete it from list
	    set_pred(next, NULL);
	    set_left(next, get_left(bp));
	    if (get_left(bp)) {
		set_parent(get_left(bp), next);
	    }
	    set_right(next, get_right(bp));
	    if (get_right(bp)) {
		set_parent(get_right(bp), next);
	    }
	    set_parent(next, get_parent(bp));
	    replace_child(get_parent(bp), bp, next);
	}
	else {
	    // Single node, delete the node
	    void *left = get_left(bp);
	    void *right = get_right(bp);
	    void *parent = get_parent(bp);
	    if (left && right) {
		// Has both left and right child
		void *minimum = tree_minimum(right);
		if (minimum == right) {
		    // Right child doesn't have left child
		    set_left(right, left);
		    set_parent(left, right);
		    set_parent(right, parent);
		    replace_child(parent, bp, right);
		}
		else {
		    // Use next minimum node to replace current node
		    if (get_right(minimum)) {
			set_parent(get_right(minimum), get_parent(minimum));
		    }
		    replace_child(get_parent(minimum), 
				  minimum, 
				  get_right(minimum));
		    set_left(minimum, left);
		    set_parent(left, minimum);
		    set_right(minimum, right);
		    set_parent(right, minimum);
		    set_parent(minimum, parent);
		    replace_child(parent, bp, minimum);
		}
	    }
	    else if (!left && right) {
		// No left child, use right child replace current node
		set_parent(right, parent);
		replace_child(parent, bp, right);
	    }
	    else if (left && !right) {
		// No right child, use left child replace current node
		set_parent(left, parent);
		replace_child(parent, bp, left);
	    }
	    else {
		// No child
		if (parent) {
		    // Leaf node
		    replace_child(parent, bp, NULL);
		}
		else {
		    // Root node
		    free_lists[LISTNUM - 1] = NULL;
		}
	    }
	}
    }
    else if (get_pred(bp)) {
	// Delete block in the list
	set_succ(get_pred(bp), get_succ(bp));
	if (get_succ(bp)) {
	    set_pred(get_succ(bp), get_pred(bp));
	}
    }
}

/*
 * bst_search - Search best fit block from bst,
 *              return the pointer to best fit block.       
 */
static void *bst_search(void *node, uint32_t size) {
    if (node == NULL) {
	return NULL;
    }
    void *fit_block;
    uint32_t cur_size = get_size(hdrp(node));
    if (size == cur_size) {
	return node;
    }
    if (size < cur_size) {
	fit_block = bst_search(get_left(node), size);
	if (fit_block) {
	    return fit_block;
	}
	return node;
    }
    else {
	return bst_search(get_right(node), size);
    }
}

/*
 * check_bst - Check bst blocks, used in mm_checkheap to check
 *             free blocks consistency.
 */
static void check_bst(void *node, int count) {
    if (node == NULL) {
	return;
    }
    check_bst(get_left(node), count);
    void *next = node;
    while (next) {
	// Check next/previous pointer consistency
	if (get_succ(next)) {
	    if (get_pred(get_succ(next)) != next) {
		puts("Next free block's pred is not this block.");
		exit(1);		    
	    }
	}
	// Check free pointer in heap
	if (!in_heap(next)) {
	    puts("Free block pointer is not in heap.");
	    exit(1);		    
	}
	// Check block is free
	if (get_alloc(hdrp(next))) {
	    puts("Allocated block is not deleted from free list.");
	    exit(1);
	}
	// Check free block fall within right node
	if (get_size(hdrp(next)) != get_size(hdrp(node))) {
	    puts("Free block is not in the right bucket.");
	    exit(1);		
	}
	next = get_succ(next);
    }
    // Check left, right child and parent
    if (get_left(node)) {
	if (get_parent(get_left(node)) != node) {
	    puts("Tree node's left child's parent isn't self.");
	    exit(1);
	}
    }
    if (get_right(node)) {
	if (get_parent(get_right(node)) != node) {
	    puts("Tree node's right child's parent isn't self.");
	    exit(1);
	}
    }
    if (get_parent(node)) {
	if (get_left(get_parent(node)) != node
	    && get_right(get_parent(node)) != node) {
	    puts("Tree node isn't its parent's child.");
	    exit(1);	    
	}
    }
    check_bst(get_right(node), count);
}

/*
 * count_bst - Count free blocks in bst, used in mm_checkheap to
 *             check the number of free blocks.
 */
static int count_bst(void *node) {
    if (node == NULL) {
	return 0;
    }
    int list_count = 0;
    void *next = node;
    while (next) {
	list_count ++;
	next = get_succ(next);
    }
    return list_count + count_bst(get_left(node)) + count_bst(get_right(node));
}

/*
 *  Malloc Implementation
 *  ---------------------
 *  The following functions deal with the user-facing malloc implementation.
 */

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4 * HWSIZE + WSIZE * LISTNUM)) == (void *) -1) {
	return -1;
    }

    heap_base = heap_listp;
    heap_listp += LISTNUM * WSIZE;    

    // Initialize segregated lists array
    free_lists = heap_base;
    memset(free_lists, 0, sizeof(void *) * LISTNUM);

    put(heap_listp, 0);                          // Alignment padding
    put(heap_listp + (1 * HWSIZE), pack(WSIZE, 1, 1, 0)); // Prologue header
    put(heap_listp + (2 * HWSIZE), pack(WSIZE, 1, 1, 0)); // Prologue footer
    put(heap_listp + (3 * HWSIZE), pack(0, 1, 1, 1));     // Epilogue header
    heap_listp += (2 * HWSIZE);
    
    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
	return -1;
    }
    return 0;
}

/*
 * malloc - Allocate a block by incrementing the brk pointer
 *          Always allocate a block whose size if a multiple of the alignment
 */
void *malloc (size_t size) {
    checkheap(1);  // Make sure the heap is ok!

    uint32_t asize;  // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;
    
    // Ignore spurious requests
    if (size == 0) {
	return NULL;
    }
    
    // Adjust block size to include overhead and alignment reqs
    if (size <= HWSIZE) {
	asize = MIN_SIZE;
    }
    else {
	asize = WSIZE * ((size + HWSIZE + (WSIZE - 1)) / WSIZE);
    }

    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
	place(bp, asize);
	return bp;
    }

    // No fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
	return NULL;
    }
    place(bp, asize);

    return bp;
}

/*
 * free - Free an allocated block
 */
void free (void *ptr) {
    if (ptr == NULL) {
	return;
    }

    if (!in_heap(ptr)) {
	printf("Cannot free block that is not in the heap.\n");
	exit(1);
    }

    uint32_t size = get_size(hdrp(ptr));
    
    // Set free block header and footer
    put(hdrp(ptr), pack(size,
                        0, 
			            get_prev_alloc(hdrp(ptr)),
			            get_prev_small(hdrp(ptr))));
    
    if (get_size(hdrp(ptr)) > MIN_SIZE) {
	put(ftrp(ptr), pack(size, 0, 0, 0));
    }

    insert_free_lists(ptr);
    
    coalesce(ptr);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *           and copying its data.
 */
void *realloc (void *oldptr, size_t size) {
    size_t oldsize;
    void *newptr;

    // If size == 0, then free the block
    if (size == 0) {
	free(oldptr);
	return NULL;
    }
    
    // If oldptr is NULL, then malloc
    if (oldptr == NULL) {
	return malloc(size);
    }

    newptr = malloc(size);
    
    // If realloc() fails the original block is left untouched
    if (!newptr) {
	return NULL;
    }

    // Copy the old data
    oldsize = get_size(hdrp(oldptr));
    if (size < oldsize) {
	oldsize = size;
    }
    memcpy(newptr, oldptr, oldsize);
    
    // Free the old block
    free(oldptr);

    return newptr;
}

/*
 * calloc - Allocate the block and set it to zero
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr;
    
    newptr = malloc(bytes);
    memset(newptr, 0, bytes);
    
    return newptr;
}

// Returns 0 if no errors were found, otherwise returns the error
int mm_checkheap (int verbose) {
    verbose = verbose;

    char* bp = next_blkp(heap_listp);
    size_t size = get_size(hdrp(bp));
    int fb_counter = 0; // Free blocks counter

    // Check heap
    while (size != 0) {
	if (!aligned(bp)) {
	    puts("Address is not 8-byte aligned.");
	    exit(1);
	}
	if (!in_heap(bp)) {
	    puts("Block is not in heap.");
	    exit(1);
	}
	// Check header in heap
	if (!in_heap(hdrp(bp))) {
	    puts("Block is not in heap.");
	    exit(1);	    
	}
	else {
	    // Check size
	    if (get_size(hdrp(bp)) < MIN_SIZE) {
		puts("Block size less than minimum size.");
		exit(1);	    		
	    }
	    // Check next header's prev allocated bit
	    if (get_alloc(hdrp(bp)) != get_prev_alloc(hdrp(next_blkp(bp)))) {
		puts("Previous allocated bit error.");
	        exit(1);
	    }
	    // Check coalescing
	    if (!get_alloc(hdrp(bp))) {
		fb_counter ++;
		if (!get_alloc(hdrp(next_blkp(bp)))) {
		    puts("Two free blocks are not coaleced.");
		    exit(1);
		}
	    }

	}
	bp = next_blkp(bp);
	size = get_size(hdrp(bp));
    } 

    // Check free list
    bp = free_lists[0];
    while (bp) {
	fb_counter --;
	bp = get_succ(bp);
    }
    for (int i = 1; i < LISTNUM - 1; i++) {
	bp = free_lists[i];
	while(bp) {
	    fb_counter --;
	    // Check next/previous pointer consistency
	    if (get_succ(bp)) {
		if (get_pred(get_succ(bp)) != bp) {
		    puts("Next free block's pred is not this block.");
		    exit(1);		    
		}
	    }
	    // Check free pointer in heap
	    if (!in_heap(bp)) {
		puts("Free block pointer is not in heap.");
		exit(1);		    
	    }
	    // Check block is free
	    if (get_alloc(hdrp(bp))) {
		puts("Allocated block is not deleted from free list.");
		exit(1);
	    }
	    // Check free block fall within right bucket
	    if (get_index(get_size(hdrp(bp))) != i) {
		puts("Free block is not in the right bucket.");
		exit(1);		
	    }
	    bp = get_succ(bp);
	}
    }

    fb_counter -= count_bst(free_lists[LISTNUM - 1]);
    if (fb_counter != 0) {
	puts("Free block number in lists doesn't match number in heap.");
	exit(1);
    }
    check_bst(free_lists[LISTNUM - 1], fb_counter);

    return 0;
}

/*
 * extend_heap - Extend heap by words (bytes)
 */
static void *extend_heap (size_t words) {
    void *bp;
    size_t size;
    
    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if (((long) (bp = mem_sbrk(size))) == -1) {
	return NULL;
    }

    // Initialize free block header/footer and the epilogue header
    put(hdrp(bp), pack(size, 
		               0, 
                       get_prev_alloc(hdrp(bp)),
                       get_prev_small(hdrp(bp))));
    uint32_t prev_small = 1;
    if (get_size(hdrp(bp)) > MIN_SIZE) {
	put(ftrp(bp), pack(size, 0, 0, 0));
	prev_small = 0;
    }

    // New epilogue header
    put(hdrp(next_blkp(bp)), pack(0, 1, 0, prev_small));    

    insert_free_lists(bp);
    
    // Coalesce if the previous block was free
    return coalesce(bp);
}

/*
 * coalesce - Coalesce potential free blocks around block bp
 */
static void *coalesce (void *bp) {
    uint32_t prev_alloc = get_prev_alloc(hdrp(bp));
    uint32_t next_alloc = get_alloc(hdrp(next_blkp(bp)));
    uint32_t size = get_size(hdrp(bp));

    if (prev_alloc && next_alloc) {
	return bp;
    }
    else if (prev_alloc && !next_alloc) {
	// Next block is free
	delete_free_lists(bp);
	delete_free_lists(next_blkp(bp));
	size += get_size(hdrp(next_blkp(bp)));
	put(hdrp(bp), pack(size, 
			           0, 
                       get_prev_alloc(hdrp(bp)),
                       get_prev_small(hdrp(bp))));
	put(ftrp(bp), pack(size, 0, 0, 0));
    }
    else if (!prev_alloc && next_alloc) {
	// Previous block is free
	void *prev = prev_blkp(bp);
	delete_free_lists(bp);
	delete_free_lists(prev);
	size += get_size(hdrp(prev));
	put(hdrp(prev), pack(size, 
			             0,
                         get_prev_alloc(hdrp(prev)),
                         get_prev_small(hdrp(prev))));
	put(ftrp(prev), pack(size, 0, 0, 0));
	bp = prev;
    }
    else {
	// Both previous and next blocks are free
	void *prev = prev_blkp(bp);
	void *next = next_blkp(bp);
	delete_free_lists(bp);
	delete_free_lists(prev);
	delete_free_lists(next);
	size += get_size(hdrp(prev)) +
	    get_size(hdrp(next));
	put(hdrp(prev), pack(size, 
			             0,
                         get_prev_alloc(hdrp(prev)),
                         get_prev_small(hdrp(prev))));
	put(ftrp(prev), pack(size, 0, 0, 0));
	bp = prev_blkp(bp);
    }
    insert_free_lists(bp);
    return bp;
}

/*
 * find_fit - Find the frist free block that fits asize
 */
static void *find_fit (size_t asize) {
    char *bp;
    char *best = NULL;
    uint32_t mSize = 0;
    uint32_t size;
    int index = get_index(asize);
    for (; index < LISTNUM - 1; index ++) {
	// Find fit block in segregated lists
	bp = free_lists[index];
	while(bp) {
	    size = get_size(hdrp(bp));
	    if (size >= asize) {
		if (size == asize) {
		    return bp;
		}
		if (!best) {
		    best = bp;
		    mSize = size;
		}
		else {
		    if (size < mSize) {
			best = bp;
			mSize = size;
		    }
		}
	    }
	    bp = get_succ(bp);
	}
	if (best) {
	    return best;
	}
    }
    
    // No fit blocks in lists, find in bst
    best = bst_search(free_lists[LISTNUM - 1], asize);

    return best;
}

/*
 * place - Set the header and footer for a newly allocated block,
 *    and the remaining free block
 */
static void place (void *bp, size_t asize) {
    uint32_t free_size = get_size(hdrp(bp));
    uint32_t remain = free_size - asize;

    if (remain < MIN_SIZE) {	
	// If remaining space is less than MIN_SIZE, merge it
	asize = free_size;
    }

    delete_free_lists(bp);
    put(hdrp(bp), pack(asize, 
		               1,
                       get_prev_alloc(hdrp(bp)),
                       get_prev_small(hdrp(bp))));
    
    if (remain >= MIN_SIZE) {
	void *next = next_blkp(bp);
	uint32_t prev_small = 0;
	if (asize <= MIN_SIZE) {
	    prev_small = 1;
	}

	put(hdrp(next), pack(remain, 0, 1, prev_small));
	if (remain > MIN_SIZE) {
	    put(ftrp(next), pack(remain, 0, 0, 0));
	}

	insert_free_lists(next);
    }
}
