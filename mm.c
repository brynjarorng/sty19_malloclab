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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// all macros from the book
// pack size and allocated bit into one word
#define PACK(size, alloc) ((size) | (alloc))

// read and write one word
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *) = (val))

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

// Pointer to first free block in the free list
char *freeBlock;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1) {
        return NULL;
    }
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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
    int numFree2 = 0;
    char *bp;

    // loop through all blocks in order, check for cycles, count free blocks
    // coalesc check, all valid addresses
    for(bp = ptr; GET_SIZE(bp) > 0; bp = NEXT_BLKP(bp)) {
        if(GET_ALLOC(bp) == 'f') {
            numFree1++;
        }

    }

    // check number of free blocks
    for(bp = freeBlock; GET_SIZE(bp) > 0 && bp != NULL; GET_NEXT_PTR(bp)) {
        if(GET_ALLOC(bp) != 'f') {
            printf("Block marked as assigned but was in free list!");
        }
        numFree2++;
    }

}

// Next pointer vs calculate next block start, speed vs space
