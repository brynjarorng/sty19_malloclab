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
int mm_check(void *ptr);
static void *extend_heap(size_t word);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// all macros from the book
// pack size and allocated bit into one word
#define PACK(size, alloc) ((size) | (alloc))

// read and write one word
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// read the size and alloc fields from addr. p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_NEXT_PTR(p) (GET(p + 0x8) & 0x8)

// compute footer and header of current block
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - ALIGNMENT)

// compute addr of next and prev block
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - ALIGNMENT)))

// Pointer to first block on heap
int *firstBlock;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // check if mem allocation was successful
    if((firstBlock = mem_sbrk(2 * ALIGNMENT)) == (void *)-1) {
        return -1;
    }

    PUT(firstBlock, 0);     // WHY?
    PUT(firstBlock + (1*WSIZE), PACK(ALIGNMENT, 1));    // Proglogue HDR
    PUT(firstBlock + (2*WSIZE), PACK(ALIGNMENT, 1));    // Prologue FTR
    PUT(firstBlock + (3*WSIZE), PACK(0, 1));            // Epilogue HDR
    firstBlock += (ALIGNMENT);

    // add blocks to the heap
    if (extend_heap(24/WSIZE) == NULL) {
        return -1;
    }

    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // alloc even number of words
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // init new free block and new epilogue HDR
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       // Adjusted block size
    size_t extendsize;  // amt. to extend heap if no fit
    char *bp;

    // ignore requests of size 0
    if (size == 0) {
        return NULL;
    }

    // Adjusted block size with overhead and allignment
    if (size <= ALIGNMENT) {
        asize = 2*ALIGNMENT;
    } else {
        asize = size + (ALIGNMENT) + (ALIGNMENT - 1);
    }

    // search for a fit on the list
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // no fit found. Extend heap
    extendsize = MAX(asize, 24);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);

    return bp;
}

static void *find_fit(size_t asize)
{
    // first fit
    void *ptr = NEXT_BLKP(HDRP(firstBlock));
    while(GET_SIZE(ptr) > 0) {
        if(!GET_ALLOC(ptr) && GET_SIZE(ptr) >= asize) {
            return ptr;
        }
        ptr =  NEXT_BLKP(ptr);
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    int freeBlockSize = GET_SIZE(bp);

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    bp = NEXT_BLKP(bp);

    // fix remeinder of free block
    PUT(HDRP(bp), PACK(freeBlockSize-asize, 0));
    PUT(FTRP(bp), PACK(freeBlockSize-asize, 0));
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

int mm_check(void *ptr)
{
    int numFree1 = 0;
    //int numFree2 = 0;
    void *bp = firstBlock;
    int memAddr[1000];
    int memAddrCounter = 0;

    // loop through all blocks in order, check for cycles, count free blocks
    // coalesc check, all valid addresses
    for(bp; GET_SIZE(bp) > 0; bp = NEXT_BLKP(bp)) {
        // check if addresses allign up with offset
        if ((int)bp % ALIGNMENT != 0) {
            printf("Alignment is incorrect!\n");
        }

        // check if address has already been added
        for (int i = 0; i < memAddrCounter && i < 1000; i++) {
            if ((int)bp == memAddr[i]) {
                printf("Address already in list!\n");
                break;
            }
        }
        memAddr[memAddrCounter++] = (int)bp;

        // check number of free blocks
        if(GET_ALLOC(bp) == 'f') {
            numFree1++;
        }

    }

    printf("%d\n", numFree1);

    // check number of free blocks
    /*
    for(bp = freeBlock; GET_SIZE(bp) > 0 && bp != NULL; GET_NEXT_PTR(bp)) {
        if(GET_ALLOC(bp) != 'f') {
            printf("Block marked as assigned but was in free list!");
        }
        numFree2++;
    }
    */
    return 0;

}
