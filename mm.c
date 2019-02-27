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
static void *extend_heap(size_t size);
static size_t *find_fit(size_t asize);
static int find_and_coalesce();
static void place(char *bp, size_t asize);

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
#define CHUNKSIZE (1<<8)
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

// get the address of the pointer block
#define FTRP(bp) (bp + GET_SIZE(bp) - WSIZE)

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
    //mm_checkheap(0);
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
        return bp + 12;
    }

    /* No fit found. Get more memory and place the block */
    
    size_t extend_size = ALIGN(MAX(block_size, CHUNKSIZE));     /* amount to extend heap if no fit */
    if ((bp = extend_heap(extend_size)) == NULL) {
        return NULL;
    }    //printf("alloc: %p\n", bp);
    place(bp, block_size);
    
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
    PUT(FTRP(bp), PACK(size, 0));

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
    // if ptr is null return new memory
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // if size == 0 free the block and return NULL
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    ptr -= 12;
    // return same pointer if large enough
    size_t old_alloc_size = GET_SIZE(ptr);
    if (old_alloc_size >= size + OVERHEAD) {
        return ptr + 12;
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
            PUT(ptr + expanded_block_size - WSIZE, PACK(expanded_block_size, 1));

            // remove block from list
            // prev->next = next
            PUT(PREV_BLKP(free_block) + WSIZE, NEXT_BLKP(free_block));
            // next->prev = prev
            PUT(NEXT_BLKP(free_block) + DSIZE, PREV_BLKP(free_block));

            return ptr + 12;
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
    memcpy(newp, ptr + 12, old_alloc_size);
    mm_free(ptr + 12);
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
    size_t remain = block_size - requested_size;

    // if block is larger than the requested block
    // either this or leave the rest to have a chance of being coalesced
    if (remain > OVERHEAD) {
        char *free_block = bp + requested_size;

        // split block and update pointers
        // change pointers pointing to the old block to point to the new block
        // prev->next = new
        PUT(PREV_BLKP(bp) + WSIZE, free_block);

        // next->prev = new
        PUT(NEXT_BLKP(bp) + DSIZE, free_block);

        // shrink the empty block
        // create header
        PUT(free_block, PACK(remain, 0));

        // create footer
        PUT(FTRP(free_block), PACK(remain, 0));

        // new->prev = bp->prev
        PUT(free_block + DSIZE, PREV_BLKP(bp));

        // new->next = bp->next
        PUT(free_block + WSIZE, NEXT_BLKP(bp));

        // fix allocated block
        // header
        PUT(bp, PACK(requested_size, 1));

        // footer
        PUT(FTRP(bp), PACK(requested_size, 1));
    } 
    else {
        // remove the block from the free list
        // prev->next = next
        PUT(PREV_BLKP(bp) + WSIZE, NEXT_BLKP(bp));

        // next->prev = prev
        PUT(NEXT_BLKP(bp) + DSIZE, PREV_BLKP(bp));

        // update header
        PUT(bp, PACK(block_size, 1));

        // update footer
        PUT(FTRP(bp), PACK(block_size, 1));
    }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static size_t *find_fit(size_t asize)
{
    void *bp = NEXT_BLKP(prologue_pointer);

    while (GET_SIZE(bp) != 0) {
        // check if block is sufficently large
        if(GET_SIZE(bp) >= (size_t)asize) { 
            return bp;
        }

        // get next block and check that
        bp = NEXT_BLKP(bp);
    }

    return NULL; // no fit
}

/* Search through list and coalesce when possible */
static int find_and_coalesce()
{
    int did_coalesce = 0;
    void *bp = PREV_BLKP(epilogue_pointer);

    while (GET_SIZE(bp - WSIZE) != 0) {
        if(!GET_ALLOC(bp) && !GET_ALLOC(bp - WSIZE)) {
            coalesce_above(bp);
            did_coalesce += 1;
        }
        bp -= GET_SIZE(bp - WSIZE);
    }
    return did_coalesce;
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
    // prev->next = below_next
    PUT(PREV_BLKP(bp) + WSIZE, NEXT_BLKP(bp));

    // next->prev = below_prev
    PUT(NEXT_BLKP(bp) + DSIZE, PREV_BLKP(bp));
    return bp - above_size;
}


void mm_checkheap(int verbose)
{
    
    int numFree2 = 0;
    void *bp = epilogue_pointer;
    int memAddrCounter = 0;
    int freeBlocksCounter = 0;
    size_t freeBlocks[1000];

    if (verbose) {
        printf("Heap (%p):\n", epilogue_pointer);
    }

    if ((GET_SIZE(bp) != 0) || !(GET_ALLOC(bp))) {
        printf("Bad epilogue header\n");
    }

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
        numFree2++;
    }

    // walk list backward
    memAddrCounter = numFree2;
    for(bp; bp == epilogue_pointer && bp != NULL; GET_PREV_PTR(bp)) {
        // check if memAddrCounter is negative. Some extra nodes in this list or loops
        if(memAddrCounter < 0) {
            printf("prev pointer list incorrect. contains loops or too many nodes!\n");
        }
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
    if ((size_t)bp % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
    }
    if (GET(bp) != GET(bp + GET_SIZE(bp) - WSIZE)) {
        printf("Error: header does not match footer\n");
    }
}
