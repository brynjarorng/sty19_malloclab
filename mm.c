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
void mm_checkheap(int verbose);
static void *extend_heap(size_t word);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define OVERHEAD 8
#define CHUNKSIZE (1<<12)
#define HDRSIZE 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// all macros from the book
// pack size and allocated bit into one word
#define PACK(size, alloc)  ((size) | (alloc))

// read and write one word
#define GET(p)         (*(size_t *)(p))
#define PUT(p, val)    (*(size_t *)(p) = (val)) 

// read the size and alloc fields from addr. p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Next ptr is 4 bytes forwards, directly behind the normal header
// prev ptr is 4 bytes after that
#define GET_NEXT_PTR(p) (GET(p + WSIZE))
#define GET_PREV_PTR(p) (GET(p + DSIZE))

// compute footer and header of current block
#define HDRP(bp) ((char *)(bp) - HDRSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - ALIGNMENT)

// compute addr of next and prev block
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - ALIGNMENT)))

// Pointer to first block on heap
static char *heap_listp;

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(8*WSIZE)) == NULL) {
        return -1;
    }

    //                   Header                 |    Footer
    //      4        +     4      +     4       +      4        = 16 bytes
    // size + alloc  -  next ptr  -  prev ptr   | size + alloc
    size_t prologue_block_size = 16;

    PUT(heap_listp, PACK(prologue_block_size, 1));             // Prologue HDR size and alloc bit
    PUT(heap_listp+WSIZE, 0);                                  // Prologue next pointer
    PUT(heap_listp+DSIZE, 0);                                  // prev pointer, always null on the head node
    PUT(heap_listp+DSIZE+WSIZE, PACK(prologue_block_size, 1)); // prologue footer

    //                   Header              
    //      4        +     4      +     4         = 12 bytes
    // size + alloc  -  next ptr  -  prev ptr
    // prologue block "will be" 0 bytes in order to know where the list stops 

    PUT(heap_listp+2*DSIZE, PACK(0,1));                     // Epilogue HDR size and alloc bit
    PUT(heap_listp+2*DSIZE+WSIZE, 0);                       // Epilogue next ptr, always null
    PUT(heap_listp+3*DSIZE, 0);                             // Epilogue prev ptr


    heap_listp += 2*DSIZE;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}
/* $end mminit */

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size) 
{  
    mm_checkheap(0);
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;      

    /* Ignore spurious requests */
    if (size <= 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) {
        asize = DSIZE + OVERHEAD;
    }
    else {
        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
    }
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
} 
/* $end mmmalloc */

/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/* $end mmfree */

/*
 * mm_realloc - naive implementation of mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newp;
    size_t copySize;

    if ((newp = mm_malloc(size)) == NULL) {
        printf("ERROR: mm_malloc failed in mm_realloc\n");
        exit(1);
    }
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newp, ptr, copySize);
    mm_free(ptr);
    return newp;
}

/* The remaining routines are internal helper routines */

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
        
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}
/* $end mmextendheap */

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{
    size_t csize = GET_SIZE(HDRP(bp));   

    if ((csize - asize) >= (DSIZE + OVERHEAD)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
    /* first fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* no fit */
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        return bp;
    }
    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }
    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

void mm_checkheap(int verbose)
{
    int numFree1 = 0;
    int numFree2 = 0;
    void *bp = heap_listp;
    int memAddr[1000];
    int memAddrCounter = 0;
    int freeBlocks[1000];
    int freeBlocksCounter = 0;

    if (verbose) {
        printf("Heap (%p):\n", heap_listp);
    }

    // check prologue and epilogue blocks 
    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
        printf("Bad prologue header\n");
    }
    checkblock(heap_listp);


    // loop through all blocks in order, check for cycles, count free blocks
    // coalesc check, all valid addresses
    for(bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // check if addresses allign up with offset
        if (verbose) {
            printblock(bp);
	    }
        checkblock(bp);

        // check number of free blocks
        if(GET_ALLOC(HDRP(bp))) {
            numFree1++;
        }

        // add all free blocks to a list
        if(GET_ALLOC(bp)) {
            freeBlocks[freeBlocksCounter++] = (size_t)bp;
        }
    }

    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
        printf("Bad epilogue header\n");
    }

    printf("%d\n", numFree1);

    // check number of free blocks in the explicit list
    // walk list forward
    for(bp = heap_listp; GET_SIZE(bp) > 0 && bp != NULL; GET_NEXT_PTR(HDRP(bp))) {
        // check if all blocks are in the free list
        // also finds blocks that are marked as allocated but are still in the free list
        for(int t = 0; t < freeBlocksCounter; t++) {
            if (freeBlocks[t] == (size_t)bp) {
                break;
            }

            // address not found in list
            if (t == freeBlocksCounter - 1) {
                printf("Address not found in the list of all free blocks!\n");
                break;
            }
        }

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
    if (numFree1 != numFree2) {
        printf("not the same number of blocks in implicit and explicit list - i: %d, e: %d\n", numFree1, numFree2);
    }

    // walk list backward
    for(bp; bp == heap_listp && bp != NULL; GET_PREV_PTR(HDRP(bp))) {
        // check if memAddrCounter is negative. Some extra nodes in this list or loops
        if(memAddrCounter < 0) {
            printf("prev pointer list incorrect. contains loops or too many nodes!\n");
        }
        
        if(memAddr[--memAddrCounter] == (size_t)bp) {
            printf("List not in same order backwards and forwards!\n");
            break;
        }
    }
    if (memAddrCounter != 0) {
         printf("note the same number of addresses up and down the list!\n");
    }

}

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
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
    if ((size_t)bp % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
    }
    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
        printf("Error: header does not match footer\n");
    }
}
