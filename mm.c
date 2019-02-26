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
static size_t *find_fit(size_t asize);
static void place(char *bp, size_t asize);

static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define OVERHEAD 16
#define CHUNKSIZE (1<<12)
// min payload space (8) + head + nxt + prv + tail = 32
#define MINCHUNKSIZE 32

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// all macros from the book
// pack size and allocated bit into one word
#define PACK(size, alloc)  ((size) | (alloc))

// read and write one word
#define GET(p)         (*(unsigned int *)(p))
#define PUT(p, val)    (*(unsigned int *)(p) = (val)) 

// read the size and alloc fields from addr. p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Next ptr is 4 bytes forwards, directly behind the normal header
// prev ptr is 4 bytes after that
#define GET_NEXT_PTR(p) (GET(p + WSIZE))
#define GET_PREV_PTR(p) (GET(p + DSIZE))

// compute addr of next and prev block
//#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define NEXT_BLKP(bp) *((size_t *)(bp + WSIZE))
#define PREV_BLKP(bp) *((size_t *)(bp + DSIZE))

inline size_t *NEXT_BLKPi(size_t *bp) {
    return *((size_t *)(bp + WSIZE));
}
inline size_t *PREV_BLKPi(size_t *bp) {
    return *((size_t *)(bp + DSIZE));
}

// Pointer to first block on heap
static char *prologue_pointer;
// pointer to epilogue
static char *epilogue_pointer;

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
    /* create the initial empty heap */
    if ((epilogue_pointer = mem_sbrk(8*WSIZE)) == NULL) {
        return -1;
    }

    // create a 4 byte padding in order to make the payloads align to 8 bits
    PUT(epilogue_pointer, 0);
    epilogue_pointer += 4;

    //                   Header                 |    Footer
    //      4        +     4      +     4       +      4        = 16 bytes
    // size + alloc  -  next ptr  -  prev ptr   | size + alloc
    // prologue and epilogue have the same structure
    size_t prologue_block_size = 16;

    PUT(epilogue_pointer, PACK(0, 1));             // Prologue HDR size and alloc bit
    PUT(epilogue_pointer+WSIZE, epilogue_pointer+2*DSIZE);                 // Prologue next pointer
    PUT(epilogue_pointer+DSIZE, 0);                                  // prev pointer, always null on the head node
    PUT(epilogue_pointer+DSIZE+WSIZE, PACK(0, 1)); // prologue footer

    // epilogue
    PUT(epilogue_pointer+2*DSIZE, PACK(0, 1));                    // Epilogue HDR size and alloc bit
    PUT(epilogue_pointer+2*DSIZE+WSIZE, 0);                       // Epilogue next ptr, always null
    PUT(epilogue_pointer+3*DSIZE, epilogue_pointer);                    // Epilogue prev ptr


    epilogue_pointer += 2*DSIZE;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(48/WSIZE) == NULL) {
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
    //mm_checkheap(0);
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;      

    /* Ignore spurious requests */
    if (size <= 0) {
        return NULL;
    }

    
    // Adjust block size to include overhead and alignment reqs.
    if (size <= DSIZE) {
        asize = MINCHUNKSIZE;
    }
    else {
        asize = ALIGN(asize);
        asize = size + DSIZE*2;
    }
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp + 12;
    }

    /* No fit found. Get more memory and place the block */
    
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(32/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    
    return bp + 12;
} 
/* $end mmmalloc */

/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    bp -= 12;
    size_t size = GET_SIZE(bp);

    // fix header and footer
    PUT(bp, PACK(size, 0));
    PUT(bp + size - WSIZE, PACK(size, 0));

    // add block to free list
    // freeblock->next = epilogoue
    PUT(bp + WSIZE, epilogue_pointer);

    // freeblock->prev = epilogue->prev
    PUT(bp + DSIZE, PREV_BLKP(epilogue_pointer));

    // epilogue->prev->next = freeblock
    PUT(PREV_BLKP(epilogue_pointer) + WSIZE, bp);

    // epilogue->prev = freeblock
    PUT(epilogue_pointer + DSIZE, bp);

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
    copySize = GET_SIZE(ptr);
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
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
        
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
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
    PUT(epilogue_pointer - WSIZE, PACK(size, 0));      // put footer

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
static void place(char *bp, size_t asize)
/* $end mmplace-proto */
{
    size_t csize = GET_SIZE(bp);
    size_t newSize = csize - asize;

    // if block is larger than the requested block
    // either this or leave the rest to have a chance of being coalesced
    if (newSize >= 15) {
        char *newBlock = bp + asize;

        // split block and update pointers
        // change pointers pointing to the old block to point to the new block
        // prev->next = new
        PUT((*(size_t *)(bp + DSIZE)) + WSIZE, newBlock);

        // next->prev = new
        PUT((*(size_t *)(bp + WSIZE)) + DSIZE, newBlock);

        // shrink the empty block
        // create header
        PUT(newBlock, PACK(newSize, 0));

        // create footer
        PUT(newBlock + newSize - WSIZE, PACK(newSize, 0));

        // new->prev = bp->prev
        PUT(newBlock + DSIZE, *(size_t *)(bp + DSIZE));

        // new->next = bp->next
        PUT(newBlock + WSIZE, *(size_t *)(bp + WSIZE));

        // fix allocated block
        // header
        PUT(bp, PACK(asize, 1));

        // footer
        PUT(bp + asize - WSIZE, PACK(asize, 1));
    }
    else {
        // remove the block from the free list
        // prev->next = next
        PUT((*(size_t *)(bp + DSIZE)) + WSIZE, *(size_t *)(bp + WSIZE));

        //next-prev = prev
        PUT((*(size_t *)(bp + WSIZE)) + DSIZE, *(size_t *)(bp + DSIZE));

        // update header and footer to be allocated
        PUT(bp, PACK(csize, 1));
        PUT(bp + csize - WSIZE, PACK(csize, 1));
    }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static size_t *find_fit(size_t asize)
{

    void *bp = *(size_t *)(epilogue_pointer + DSIZE);

    while (GET_SIZE(bp) != 0) {
        // check if block is sufficently large and not allocated
        int tmpSize = GET_SIZE(bp);
        if(GET_SIZE(bp) >= (int)asize) {
            return bp;
        }

        // get prev block and check that
        bp = *(size_t *)(bp + DSIZE);
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
    //printf("pa: %p, p: %d, n: %d\n", bp, prev_alloc, next_alloc);
    //fflush(stdout);

    if (prev_alloc && next_alloc) {            // Case 1 - Nothing
        return bp;
    }
    if (!prev_alloc && next_alloc) {      // Case 2 - Free above
        coalesce_above(bp);
    }
    if (prev_alloc && !next_alloc) {      // Case 3 - Free below
        coalesce_above(bp + size);
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
    PUT(bp + size - WSIZE, PACK(size + above_size, 0));

    // remove below block from free list
    // prev->next = below_next
    PUT(PREV_BLKP(bp) + WSIZE, *((size_t *)(bp + WSIZE)));

    // next->prev = below_prev
    PUT(NEXT_BLKP(bp) + DSIZE, *((size_t *)(bp + DSIZE)));
}


void mm_checkheap(int verbose)
{
    int numFree1 = 0;
    int numFree2 = 0;
    void *bp = epilogue_pointer;
    int memAddr[1000];
    int memAddrCounter = 0;
    int freeBlocks[1000];
    int freeBlocksCounter = 0;

    if (verbose) {
        printf("Heap (%p):\n", epilogue_pointer);
    }

    // check prologue and epilogue blocks 
    if ((GET_SIZE(epilogue_pointer) != DSIZE) || !GET_ALLOC(epilogue_pointer)) {
        printf("Bad prologue header\n");
    }
    checkblock(epilogue_pointer);


    // loop through all blocks in order, check for cycles, count free blocks
    // coalesc check, all valid addresses
    for(bp; GET_SIZE(bp) > 0; bp = NEXT_BLKP(bp)) {
        // check if addresses allign up with offset
        if (verbose) {
            printblock(bp);
	    }
        checkblock(bp);

        // check number of free blocks
        if(GET_ALLOC(bp)) {
            numFree1++;
        }

        // add all free blocks to a list
        if(GET_ALLOC(bp)) {
            freeBlocks[freeBlocksCounter++] = (size_t)bp;
        }
    }

    if ((GET_SIZE(bp) != 0) || !(GET_ALLOC(bp))) {
        printf("Bad epilogue header\n");
    }

    printf("%d\n", numFree1);

    // check number of free blocks in the explicit list
    // walk list forward
    for(bp = epilogue_pointer; GET_SIZE(bp) > 0 && bp != NULL; GET_NEXT_PTR(bp)) {
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
    for(bp; bp == epilogue_pointer && bp != NULL; GET_PREV_PTR(bp)) {
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

    hsize = GET_SIZE(bp);
    halloc = GET_ALLOC(bp);  
    //fsize = GET_SIZE(FTRP(bp));
    //falloc = GET_ALLOC(FTRP(bp));  
    
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
    /*if (GET(bp) != GET(FTRP(bp))) {
        printf("Error: header does not match footer\n");
    }*/
}
