/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below _AND_ in the
 * struct that follows.
 *
 * Note: This comment is parsed. Please do not change the
 *       Format!
 *
 * === User information ===
 * Group: 
 * User 1: 
 * SSN: X
 * User 2: 
 * SSN: X
 * User 3: 
 * SSN: X
 * === End User Information ===
 ********************************************************/
team_t team = {
    /* Group name */
    "Nuclear Ghandi",
    /* First member's full name */
    "Brynjar Örn Grétarsson",
    /* First member's email address */
    "brynjarog17@ru.is",
    /* Second member's full name (leave blank if none) */
    "Daði Steinn Brynjarsson",
    /* Second member's email address (leave blank if none) */
    "dadib17@ru.is",
    /* Third full name (leave blank if none) */
    "",
    /* Third member's email address (leave blank if none) */
    ""
};

// function decleration
void mm_checkheap(int verbose, int full_check);
static void *extend_heap(size_t size);
static size_t *find_fit(size_t asize);
static void place(char *bp, size_t asize);
static void push(char *bp);

static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define HDRSIZE 12          //size of HDR
#define OVERHEAD 16
#define MINBLOCKSIZE 32
#define SMBLCKSIZE (1<<10)
#define CHUNKSIZE (1<<12)
// min payload space (8) + head + nxt + prv + tail = 32

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size_t)(size) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// all macros from the book
// pack size and allocated bit into one word
#define PACK(size, alloc)  ((size) | (alloc))

// read and write one word
#define GET(p)         (*(unsigned int *)(p))
#define PUT(p, val)    (*(unsigned int *)(p) = (val)) 

// read the size and alloc fields from addr. p
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

// compute addr of next and prev block
//#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define NEXT_BLKP(bp) *((size_t *)(bp + WSIZE))
#define PREV_BLKP(bp) *((size_t *)(bp + DSIZE))

// Pointer manipulation
#define PREV_BLKP(bp)   *((size_t *)(bp + DSIZE))     //prev pointer on free list
#define NEXT_BLKP(bp)   *((size_t *)(bp + WSIZE))     //next pointer on free list
#define ABOVE_BLKP(bp)  ((char *)bp - GET_SIZE(bp))  //prev block on heap
#define BELOW_BLKP(bp)  ((char *)bp + GET_SIZE(bp))  //next block on heap
#define FTRP(bp)         (BELOW_BLKP(bp) - WSIZE)

// Pointers to prologue and epilogue
static char *prologue_pointer;
static char *epilogue_pointer;

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
    /* create the initial empty heap */
    if ((prologue_pointer = mem_sbrk(8*WSIZE)) == NULL) {
        return -1;
    }

    // create a 4 byte padding in order to make the payloads align to 8 bits
    PUT(prologue_pointer, 0);

    //                   Header                 |    Footer
    //      4        +     4      +     4       +      4        = 16 bytes
    // size + alloc  -  next ptr  -  prev ptr   | size + alloc
    // prologue and epilogue have the same structure
    
    // setting initial prologue and epilogue pointers
    prologue_pointer += 4;
    epilogue_pointer = prologue_pointer + OVERHEAD;

    // Prologue
    PUT(prologue_pointer, PACK(0, 1));                  // Prologue HDR size and alloc bit
    PUT(prologue_pointer+WSIZE, epilogue_pointer);      // Prologue next pointer
    PUT(prologue_pointer+DSIZE, 0);                     // prev pointer, always null on the head node
    PUT(prologue_pointer+HDRSIZE, PACK(0, 1));      // prologue footer

    // Epilogue
    PUT(epilogue_pointer, PACK(0, 1));                    // Epilogue HDR size and alloc bit
    PUT(epilogue_pointer+WSIZE, 0);                       // Epilogue next ptr, always null
    PUT(epilogue_pointer+DSIZE, prologue_pointer);                    // Epilogue prev ptr

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE) == NULL) {
        return -1;
    }
    return 0;
}
/* $end mminit */

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
/* $begin mmmalloc */
void *mm_malloc(size_t pl_size) 
{
    size_t block_size;      /* adjusted block size */
    char *bp;      

    /* Ignore spurious requests */
    if (pl_size <= 0) {
        return NULL;
    }

    // Adjust block size to include overhead and alignment reqs.
    block_size = MAX(ALIGN(pl_size + OVERHEAD), MINBLOCKSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(block_size)) != NULL) {
        place(bp, block_size);
        return bp + HDRSIZE;
    }

    /* No fit found. Get more memory and place the block */
    
    size_t extend_size = ALIGN(MAX(block_size, CHUNKSIZE));     /* amount to extend heap if no fit */
    if ((bp = extend_heap(extend_size)) == NULL) {
        return NULL;
    }
    place(bp, block_size);
    
    return bp + HDRSIZE;
} 
/* $end mmmalloc */

/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    bp -= HDRSIZE;
    size_t block_size = GET_SIZE(bp);

    // fix header and footer
    PUT(bp, PACK(block_size, 0));
    PUT(FTRP(bp), PACK(block_size, 0));
    
    // Insert block to free list
    mm_insert(bp);

    coalesce(bp);
}

/* $end mmfree */

// Insert block into free list
void mm_insert(void *bp)
{

    // Small block are added to the back of the list
    if(GET_SIZE(bp) <= SMBLCKSIZE) {
        // Fix free block pointers
        // new->next = epilogue
        PUT(bp + WSIZE, epilogue_pointer);
        // new->prev = epilogue->prev
        PUT(bp + DSIZE, PREV_BLKP(epilogue_pointer));

        // Connect free block to back of free list
        // epilogue->prev->next = newblock
        PUT(PREV_BLKP(epilogue_pointer) + WSIZE, bp);
        // epilogue->prev = newblock
        PUT(epilogue_pointer + DSIZE, bp);
    }

    // Other blocks are added to the front of the list
    else {
        // Fix new block pointers
        // new->prev = prologue
        PUT(bp + DSIZE, prologue_pointer);
        // new->next = prologue->prev
        PUT(bp + WSIZE, NEXT_BLKP(prologue_pointer));

        // Connect free block to front of free list
        // prologue->next->prev = newblock
        PUT(NEXT_BLKP(prologue_pointer) + DSIZE, bp);
        // prologue->next = newblock
        PUT(prologue_pointer + WSIZE, bp);
    }
}

// Remove block from free list (coalesce)
void mm_remove(void *bp)
{
    // prev->next = next
    PUT(PREV_BLKP(bp) + WSIZE, NEXT_BLKP(bp));
    // next->prev = prev
    PUT(NEXT_BLKP(bp) + DSIZE, PREV_BLKP(bp));
}

/*
 * mm_realloc - naive implementation of mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    mm_checkheap(0, 0);
    // if ptr is null return new memory
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // if size == 0 free the block and return NULL
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    ptr -= HDRSIZE;
    // return same pointer if large enough
    size_t old_alloc_size = GET_SIZE(ptr);
    if (old_alloc_size >= size + OVERHEAD) {
        return ptr + HDRSIZE;
    }
    
    // check if block behind is free and large enough for the realloc
    if (!GET_ALLOC(ptr + old_alloc_size)) {
        size_t block_behind_size = GET_SIZE(ptr + old_alloc_size);
        size_t expanded_block_size = old_alloc_size + block_behind_size;
        if (expanded_block_size >= size + OVERHEAD) {
            void *free_block = ptr + old_alloc_size;

            // change header
            PUT(ptr, PACK(expanded_block_size, 1));
            // change footer
            PUT(FTRP(ptr), PACK(expanded_block_size, 1));

            // remove block from list
            mm_remove(free_block);

            return ptr + HDRSIZE;
        }
    }

    
    // else get a new block, copy old content, free the block then return new pointer
    void *newp;

    if ((newp = mm_malloc(size)) == NULL) {
        printf("ERROR: mm_malloc failed in mm_realloc\n");
        exit(1);
    }
    old_alloc_size -= OVERHEAD;
    if (size < old_alloc_size) {
        old_alloc_size = size;
    }
    memcpy(newp, ptr + HDRSIZE, old_alloc_size);
    mm_free(ptr + HDRSIZE);
    return newp;
}

/* The remaining routines are internal helper routines */

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t size) 
{
    char *bp;

    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }
    bp = epilogue_pointer;
    // move epilogue pointer to new epilogue block
    epilogue_pointer += size;

    // init new block
    PUT(bp, PACK(size, 0));                            // put header
    PUT(bp + WSIZE, epilogue_pointer);                 // next pointer
    // prev pointer does not need to be changed, it still points to a valid block
    PUT(FTRP(bp), PACK(size, 0));                      // put footer

    // write new epilogue block
    PUT(epilogue_pointer, PACK(0, 1));                  // put header
    PUT(epilogue_pointer + WSIZE, 0);                   // next pointer
    PUT(epilogue_pointer + DSIZE, bp);                  // prev pointer

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(char *bp, size_t requested_size)
/* $end mmplace-proto */
{
    size_t block_size = GET_SIZE(bp);
    size_t remaining_size = block_size - requested_size;
    
    // remove the block from the free list
    mm_remove(bp);
    
    // if block is larger than the requested block
    // either this or leave the rest to have a chance of being coalesced
    if (remaining_size > MINBLOCKSIZE) {
        char *free_block = bp + requested_size;

        // Set allocated header
        PUT(bp, PACK(requested_size, 1));
        // Set allocated Footer
        PUT(FTRP(bp), PACK(requested_size, 1));

        // Set free header
        PUT(free_block, PACK(remaining_size, 0));
        // Set free footer
        PUT(FTRP(free_block), PACK(remaining_size, 0));

        // Insert free block to free list
        mm_insert(free_block);        
    } 
    else {
        // update header
        PUT(bp, PACK(block_size, 1));
        // update footer
        PUT(FTRP(bp), PACK(block_size, 1));
    }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for the size of the block
 */
static size_t *find_fit(size_t block_size)
{
    if (block_size <= SMBLCKSIZE) {
        void *bp = NEXT_BLKP(prologue_pointer);

        while (GET_SIZE(bp) != 0) {
            // check if block is sufficently large
            if(GET_SIZE(bp) >= block_size) { 
                return bp;
            }

            // get next block and check that
            bp = NEXT_BLKP(bp);
        }
    }
    else {
        void *bp = PREV_BLKP(epilogue_pointer);

        while (GET_SIZE(bp) != 0) {
            // check if block is sufficently large
            if(GET_SIZE(bp) >= block_size) { 
                return bp;
            }

            // get next block and check that
            bp = PREV_BLKP(bp);
        }
    }

    return NULL; // no fit
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(bp - WSIZE);
    size_t size = GET_SIZE(bp);
    size_t next_alloc = GET_ALLOC(bp + size);

    if (prev_alloc && next_alloc) {            // Case 1 - Nothing
        return bp;
    }
    else if (!prev_alloc && next_alloc) {      // Case 2 - Free above
        size_t tmp_size = GET_SIZE(bp - WSIZE);
        coalesce_above(bp);
        bp -= tmp_size;
    }
    else if (prev_alloc && !next_alloc) {      // Case 3 - Free below
        coalesce_above(bp + size);
    } else {
        size_t tmp_size = GET_SIZE(bp - WSIZE);
        coalesce_above(bp + size);
        coalesce_above(bp);
        bp -= tmp_size;
    }
    
    return bp;
}

// takes in a 
void coalesce_above(void *bp)
{
    size_t size = GET_SIZE(bp);
    size_t above_size = GET_SIZE(bp - WSIZE);

    // fix header and footer
    PUT(bp - above_size, PACK(size + above_size, 0));
    PUT(FTRP(bp), PACK(size + above_size, 0));

    // remove below block from free list
    mm_remove(bp);

    return bp - above_size;
}


void mm_checkheap(int verbose, int full_check)
{
    size_t numFree1 = 0;
    size_t numFree2 = 0;
    void *bp = prologue_pointer;
    size_t memAddr[100000];
    size_t memAddrCounter = 0;

    if (verbose) {
        printf("Heap (%p):\n", prologue_pointer);
    }

    // check prologue and epilogue blocks 
    if ((GET_SIZE(prologue_pointer) != 0) || !GET_ALLOC(prologue_pointer)) {
        printf("Bad prologue header\n");
    }
    
    // only walk through entire heap with small test files, else it will take FOREVER!
    if (full_check) {
        // loop through all blocks in order, check for cycles, count free blocks
        // coalesc check, all valid addresses
        for(bp; GET_SIZE(bp + WSIZE) > 0; bp += GET_SIZE(bp)) {
            // check if addresses allign up with offset
            if (verbose) {
                printblock(bp);
            }
            
            if(GET_SIZE(bp)) {
                checkblock(bp);
            }

            // check number of free blocks
            if(GET_ALLOC(bp)) {
                numFree1++;
            }

            // check if memory address is valid
            if (bp < prologue_pointer || bp > epilogue_pointer) {
                printf("%p Memory address lies outside of heap!\n");
            }
        }
    }
    

    if ((GET_SIZE(epilogue_pointer) != 0) || !(GET_ALLOC(epilogue_pointer))) {
        printf("Bad epilogue header\n");
    }

    // print out number of free block in
    if (verbose) {
        printf("%d\n", numFree1);
    }
    
    // check number of free blocks in the explicit list
    // walk list forward
    for(bp = NEXT_BLKP(prologue_pointer); GET_SIZE(bp) > 0 && bp != NULL; bp = NEXT_BLKP(bp)) {
        // check if memory address is valid
        if (bp < prologue_pointer || bp > epilogue_pointer) {
            printf("%p Memory address lies outside of heap!\n");
        }
        
        if (verbose) {
            printblock(bp);
	    }

        // validate that header is the same as the footer
        if(GET_SIZE(bp)) {
            checkblock(bp);
        }

        // check blocks that are marked as allocated but are still in the free list

        // check if address has already been added
        for (int i = 0; i < memAddrCounter; i++) {
            if ((size_t)bp == memAddr[i]) {
                printf("%p: Address already in list!\n", bp);
                break;
            }
        }

        numFree2++;
        memAddr[memAddrCounter++] = (size_t)bp;
    }
    
    // check if implicit and explicit list count the same number of free blocks
    if (full_check && numFree1 != numFree2) {
        printf("not the same number of blocks in implicit and explicit list - i: %d, e: %d\n", numFree1, numFree2);
    }
    
    // walk list backward
    for(bp = PREV_BLKP(bp); GET_SIZE(bp) > 0 && bp != NULL; bp = PREV_BLKP(bp)) {
        // check if memory address is valid
        if (bp < prologue_pointer || bp > epilogue_pointer) {
            printf("%p Memory address lies outside of heap!\n");
        }
        
        // check if memAddrCounter is negative. Some extra nodes in this list or loops
        if(memAddrCounter < 0) {
            printf("prev pointer list incorrect. contains loops or too many nodes!\n");
            break;
        }
        
        if(memAddr[--memAddrCounter] != (size_t)bp) {
            printf("List not in same order backwards and forwards!\n");
            break;
        }
    }
    if (memAddrCounter != 0) {
         printf("note the same number of addresses up and down the list! N: %d\n", memAddrCounter);
    }
}

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(bp);
    halloc = GET_ALLOC(bp);  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));
      
    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, 
           hsize, (halloc ? 'a' : 'f'), 
           fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
    if ((size_t)(bp + HDRSIZE) % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
    }
    if (GET_ALLOC(bp) != GET_ALLOC(FTRP(bp))) {
        printf("Error: header alloc does not match footer alloc. H: %d, F: %d, Size: %d\n", GET_ALLOC(bp), GET_ALLOC(FTRP(bp)));
    }
    if (GET_SIZE(bp) != GET_SIZE(FTRP(bp))) {
        printf("Error: header size does not match footer size\n");
    }
    
}