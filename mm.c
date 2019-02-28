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
 * Group: Nuclear Ghandi
 * User 1: brynjarog17
 * SSN: 270498-3029
 * User 2: dadib17
 * SSN: 140990-2729
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
static void set_hdr_ftr(char *bp, size_t block_size, int alloc);

static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define HDRSIZE 12              // size of HDR
#define OVERHEAD 16             // size of HDR(12) + FTR(4)
#define MINBLOCKSIZE 32         // Min block to allocate
#define SMBLCKSIZE (1<<10)      // Used to add to and search the free list
#define CHUNKSIZE (1<<12)       // Used for heap extension

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size_t)(size) + (ALIGNMENT-1)) & ~0x7)
#define MAX(x, y) ((x) > (y)? (x) : (y))

// all macros from the book
// pack size and allocated bit into one word
#define PACK(size, alloc)  ((size) | (alloc))

// read and write one word
#define GET(p)         (*(unsigned int *)(p))
#define PUT(p, val)    (*(unsigned int *)(p) = (val)) 

// read the size and alloc fields from addr. p
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

// Pointer manipulation
#define PREV_BLKP(bp)   *((size_t *)(bp + DSIZE))     //prev pointer on free list
#define NEXT_BLKP(bp)   *((size_t *)(bp + WSIZE))     //next pointer on free list
#define ABOVE_BLKP(bp)  ((char *)bp - GET_SIZE(bp))  //prev block on heap
#define BELOW_BLKP(bp)  ((char *)bp + GET_SIZE(bp))  //next block on heap
#define FTRP(bp)         (BELOW_BLKP(bp) - WSIZE)

// Pointers to prologue and epilogue
static char *prol_ptr;
static char *epil_ptr;

/*
        The free list is an explicit doubly linked list where only the free blocks are in it. 
        Blocks smaller than SMBLCKSIZE get put at the back of the list while larger blocks are put
        at the front. The fint_fit function looks for blocks smaller than SMBLCKSIZE starting at the
        end of the list and working it's way from there while other blocks are taken from the front of the list.

        This implementation uses a 12 byte header and 4 byte footer for all the blocks on the list. The payload
        starts 12 bytes in the block. In order to make it aligned the list starts with a single 4 byte block before
        the prologue block and all blocks and memory expansions are a multiplication of 8 bytes in size.
        Therefore we do not need to think about alignment when placing the blocks.

        mm_free(size_t ptr) marks an incoming block as free puts blocks smaller than SMBLCKSIZE at the end of the
        list and other blocks at the start.

        our block implementation:
                                      | Address aligned by 8 bytes
        --------------------------------------------------
        | 4 bytes | 4 bytes | 4 bytes | N bytes | 4 bytes|
        | alloc & | nxt ptr | prv ptr | payload | alloc  |
        | size    |         |         | N%8 = 0 | size   |
        --------------------------------------------------
*/

/* 
 * mm_init - Initialize the memory manager 
 * 
 * Initializes the heap by initializing the prologue block and epilogue block by setting them to
 * have their allocated bit set and their size set to 0. Then make the next pointer on the prologue block
 * point to the epilogue block and the prev pointer be null. Make the prev pointer on the epilogue block
 * point to the prologue block and make the next pointer be null. Then extend the heap to it's init size.
 * 
 *                    Header                 |      Footer
 *        4        +     4      +     4        +      4        = 16 bytes
 *  size + alloc  -  next ptr  -  prev ptr   |   size + alloc
 *  prologue and epilogue have the same structure EXCEPT that the epilogue block does not have
 *  a footer because it does not need it. It will never be accessed using this implementation.
 */
int mm_init(void) 
{
    /* create the initial empty heap */
    if ((prol_ptr = mem_sbrk(8*WSIZE)) == NULL) {
        return -1;
    }

    // create a 4 byte padding in order to make the payloads align to 8 bits
    PUT(prol_ptr, 0);
    
    // setting initial prologue and epilogue pointers
    prol_ptr += 4;
    epil_ptr = prol_ptr + OVERHEAD;

    // Prologue
    PUT(prol_ptr, PACK(0, 1));                  // Prologue HDR size and alloc bit
    PUT(prol_ptr+WSIZE, epil_ptr);              // Prologue next pointer
    PUT(prol_ptr+DSIZE, 0);                     // prev pointer, always null on the head node
    PUT(prol_ptr+HDRSIZE, PACK(0, 1));          // prologue footer

    // Epilogue
    PUT(epil_ptr, PACK(0, 1));                    // Epilogue HDR size and alloc bit
    PUT(epil_ptr+WSIZE, 0);                       // Epilogue next ptr, always null
    PUT(epil_ptr+DSIZE, prol_ptr);                // Epilogue prev ptr

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload
 * 
 * Ignores invalid blocks iszes by returning NULL. Finds a block that is big enough to fit the request and is 8 byte aligned.
 * Either align the requested block size to be a multiple of 8 by only making it larger or use the MINBLOCKSIZE. Pick the larger of
 * the two choices. Search for a suitibly large free block and put the block in that space. The precise functionality of the functions
 * place() and find_fit() is described below in the respective function headers.
 * 
 */
void *mm_malloc(size_t pl_size) 
{
    size_t block_size;      /* adjusted block size */
    char *hdr_ptr;

    /* Ignore spurious requests */
    if (pl_size <= 0) {
        return NULL;
    }

    // Adjust block size to include overhead and alignment reqs.
    block_size = MAX(ALIGN(pl_size + OVERHEAD), MINBLOCKSIZE);
    
    /* Search the free list for a fit */
    if ((hdr_ptr = find_fit(block_size)) != NULL) {
        place(hdr_ptr, block_size);
        return hdr_ptr + HDRSIZE;
    }

    /* No fit found. Get more memory and place the block */
    size_t extend_size = MAX(block_size, CHUNKSIZE);     /* amount to extend heap if no fit */
    if ((hdr_ptr = extend_heap(extend_size)) == NULL) {
        return NULL;
    }
    place(hdr_ptr, block_size);
    
    return hdr_ptr + HDRSIZE;
}

/* 
 * mm_free - Free a block 
 * 
 * Set the block header and footer to unallocated and insert the block into the free list. Then coalesce if possible.
 */
void mm_free(void *bp)
{
    // Move ptr from the payload to the beginning of the block
    bp -= HDRSIZE;
    size_t block_size = GET_SIZE(bp);

    // fix header and footer
    set_hdr_ftr(bp, block_size, 0);

    // Insert block to free list
    mm_insert(bp);

    coalesce(bp);
}

/*
 * mm_realloc - naive implementation of mm_realloc
 * 
 * Returns a new block if pointer is a Null pointer, Returns NULL nd frees a pointer if new size is 0. Returns
 * the same pointer if current block can accumulate the new requested block size. If requested size is larger than
 * the current block, check if the block directly behind in memory is free and large enough to accomidate the new requested block size.
 * If so change the headder of the current block to be of the size of the current block plus next block and fix the footer of that
 * block to match the header and return the same pointer. Else if nothing aforementioned is applicable, call mm_malloc 
 * to get a new appropriately sized block, copy all information from the old block to the new block, free the old block and return the new block.
 */
void *mm_realloc(void *pl_ptr, size_t size)
{
    mm_checkheap(0, 0);
    // If ptr is null, return new memory
    if (pl_ptr == NULL) {
        return mm_malloc(size);
    }

    // If size == 0, free the block and return NULL
    if (size == 0) {
        mm_free(pl_ptr);
        return NULL;
    }

    // Move pointer to HDR
    void *hdr_ptr = pl_ptr - HDRSIZE;

    // Return same pointer if large enough
    size_t old_alloc_size = GET_SIZE(hdr_ptr);
    if (old_alloc_size >= size + OVERHEAD) {
        return pl_ptr;
    }
    
    // Check if block behind is free and can be used for expanding the current block
    if (!GET_ALLOC(hdr_ptr + old_alloc_size)) {
        size_t block_behind_size = GET_SIZE(hdr_ptr + old_alloc_size);
        size_t expanded_block_size = old_alloc_size + block_behind_size;
        
        // Check if expanded block (old block + free block below) is large enough for realloc
        if (expanded_block_size >= size + OVERHEAD) {
            void *free_block = hdr_ptr + old_alloc_size;

            // Set HDR and FTR
            set_hdr_ftr(hdr_ptr, expanded_block_size, 1);

            // Remove block from list
            mm_remove(free_block);

            return pl_ptr;
        }
    }
    
    // Else get a new block, copy old content, free the block then return new pointer
    void *newp;

    // Get new block
    if ((newp = mm_malloc(size)) == NULL) {
        printf("ERROR: mm_malloc failed in mm_realloc\n");
        exit(1);
    }
    
    // Copy paylod to new block
    memcpy(newp, pl_ptr, size);
    
    // Free old block
    mm_free(pl_ptr);
    
    return newp;
}

/* The remaining routines are internal helper routines */


/* 
 * mm_insert - Insert block into free list
 * 
 * If a block is smaller than SMBLCKSIZE it is put at the end of the free lest just in front of the prologue block.
 * Else it is put at the front of the list. The prev pointer in the epilogue block points to the new block, the next pointer 
 * in the old block that was behind the epilogue block also points to the new block. The new blocks next points to the epilogue block
 * and the previous pointer points to the old block. The same applies to inserting at the front of the list except the next pointer
 * is changed in th prologue pointer and the prev pointer is changed in the old first usable block.
 */
void mm_insert(void *hdr_ptr)
{
    // Small block are added to the back of the list
    if(GET_SIZE(hdr_ptr) <= SMBLCKSIZE) {
        // Fix free block pointers
        // new->next = epilogue
        PUT(hdr_ptr + WSIZE, epil_ptr);
        // new->prev = epilogue->prev
        PUT(hdr_ptr + DSIZE, PREV_BLKP(epil_ptr));

        // Connect free block to back of free list
        // epilogue->prev->next = newblock
        PUT(PREV_BLKP(epil_ptr) + WSIZE, hdr_ptr);
        // epilogue->prev = newblock
        PUT(epil_ptr + DSIZE, hdr_ptr);
    }

    // Other blocks are added to the front of the list
    else {
        // Fix new block pointers
        // new->prev = prologue
        PUT(hdr_ptr + DSIZE, prol_ptr);
        // new->next = prologue->prev
        PUT(hdr_ptr + WSIZE, NEXT_BLKP(prol_ptr));

        // Connect free block to front of free list
        // prologue->next->prev = newblock
        PUT(NEXT_BLKP(prol_ptr) + DSIZE, hdr_ptr);
        // prologue->next = newblock
        PUT(prol_ptr + WSIZE, hdr_ptr);
    }
}

/* 
 * mm_remove - Remove block from free list
 * 
 * Short function that removes a single block from the free list. It changes the pointers of the block behind
 * to point to the next block and the block in front to point to the block behind. Effectively pulling the block out of the list.
 */
void mm_remove(void *hdr_ptr)
{
    // prev->next = next
    PUT(PREV_BLKP(hdr_ptr) + WSIZE, NEXT_BLKP(hdr_ptr));
    // next->prev = prev
    PUT(NEXT_BLKP(hdr_ptr) + DSIZE, PREV_BLKP(hdr_ptr));
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 * 
 * Extend heap asks mem_sbrk to allocate more memory to the checker. All request are a multiplication of 8
 * in order to make it easy to maintain 8 byte alignment.
 * 
 * First the old epilogue header is overwritten and a new footer is placed "size" bytes down and
 * the block is marked as free. Just below that a new epilogue block is created and added to the
 * end of the free list. There is no need to change any pointers, only to add a new next pointer to the 
 * new free block and a prev pointer to the epilogue block. End by trying to coalesce.
 */
static void *extend_heap(size_t size) 
{
    char *bp;

    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }
    
    bp = epil_ptr;
    // move epilogue pointer to new epilogue block
    epil_ptr += size;
    
    // init new block
    set_hdr_ftr(bp, size, 0);                  // Set HDR and FTR
    PUT(bp + WSIZE, epil_ptr);                 // next pointer
    // prev pointer does not need to be changed, it still points to a valid block

    // write new epilogue block
    PUT(epil_ptr, PACK(0, 1));                  // put header
    PUT(epil_ptr + WSIZE, 0);                   // next pointer
    PUT(epil_ptr + DSIZE, bp);                  // prev pointer

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 * 
 * If a block that is going to be placed leaves less than MINBLOCKSIZE bytes of memory
 * it takes all of the remaining memory and removes the block from the freelist. Else it needs
 * to split the block into an allocated block and free block. The allocated block gets new a header
 * and footer while the new free block also gets added to the free list.
 */
static void place(char *bp, size_t requested_size)
{
    size_t block_size = GET_SIZE(bp);
    size_t remaining_size = block_size - requested_size;
    
    // remove the block from the free list
    mm_remove(bp);
    
    // if block is larger than the requested block
    // either this or leave the rest to have a chance of being coalesced
    if (remaining_size > MINBLOCKSIZE) {
        char *free_ptr = bp + requested_size;

        // Set HDR and FTR for allocated block
        set_hdr_ftr(bp, requested_size, 1);

        // Set HDR and FTR for free block
        set_hdr_ftr(free_ptr, remaining_size, 0);

        // Insert free block to free list
        mm_insert(free_ptr);        
    } 
    else {
        // Update HDR and FTR
        set_hdr_ftr(bp, block_size, 1);
    }
}

/*
 * set_hdr_ftr - set the allocation bit and size of a block header and footer
 */
static void set_hdr_ftr(char *hdr_ptr, size_t block_size, int alloc)
{
    // set header and footer
    PUT(hdr_ptr, PACK(block_size, alloc));
    PUT(FTRP(hdr_ptr), PACK(block_size, alloc));
}

/* 
 * find_fit - Find a fit for the size of the block
 * 
 * If a block is smaller than SMBLCKSIZE, start at the end of the free list and search in reverse order.
 * Else start at the start of the free list. If a large enough block is found, return a pointer to it
 * else return NULL 
 */
static size_t *find_fit(size_t block_size)
{
    // Search list backwards for small blocks
    if (block_size <= SMBLCKSIZE) {
        void *hdr_ptr = NEXT_BLKP(prol_ptr);

        while (GET_SIZE(hdr_ptr) != 0) {
            // Check if block is sufficently large
            if(GET_SIZE(hdr_ptr) >= block_size) { 
                return hdr_ptr;
            }

            // Get next block and check that
            hdr_ptr = NEXT_BLKP(hdr_ptr);
        }
    }
    // Search blocks forward for other blocks
    else {
        void *hdr_ptr = PREV_BLKP(epil_ptr);

        while (GET_SIZE(hdr_ptr) != 0) {
            // Check if block is sufficently large
            if(GET_SIZE(hdr_ptr) >= block_size) { 
                return hdr_ptr;
            }

            // Get next block and check that
            hdr_ptr = PREV_BLKP(hdr_ptr);
        }
    }
    // no fit
    return NULL;
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 * 
 * Check four cases that can come up:
 * 1. If both block above and below are not free, return and do nothing
 * 2. If block above is free, get the size of that block, coalesce it with the current block and return a pointer
 *    to the block above
 * 3. If block below is free, call coalesce_above with the pointer of the block 
 *    below and return the pointer to the block above
 * 4. If both block above and below are free, get the size of the block above, then coalesce the block below lastly
 *    coalesce the block above and return a pointer to the top most block
 */
static void *coalesce(void *hdr_ptr) 
{
    size_t size = GET_SIZE(hdr_ptr);
    size_t prev_alloc = GET_ALLOC(hdr_ptr - WSIZE);
    size_t next_alloc = GET_ALLOC(hdr_ptr + size);

    if (prev_alloc && next_alloc) {                         // Case 1 - Nothing
        return hdr_ptr;
    }
    else if (!prev_alloc && next_alloc) {                   // Case 2 - Free above
        size_t tmp_size = GET_SIZE(hdr_ptr - WSIZE);
        coalesce_above(hdr_ptr);
        hdr_ptr -= tmp_size;
    }
    else if (prev_alloc && !next_alloc) {                   // Case 3 - Free below
        coalesce_above(hdr_ptr + size);
    } 
    else {
        size_t tmp_size = GET_SIZE(hdr_ptr - WSIZE);        // Case 4 - Free above and below
        coalesce_above(hdr_ptr + size);
        coalesce_above(hdr_ptr);
        hdr_ptr -= tmp_size;
    }
    
    return hdr_ptr;
}

/*
 * coalesce_above - coalesce current block with block above
 *
 * takes in a pointer to a free block that has another free block above it in memory and merges them togeather
 * by changing the size of the footer of this block and the header of the block above the size of the
 * current block plus the block above. Then it calls mm_remove to remove the current block (lower block of the two)
 * from the free list
 */
void coalesce_above(void *hdr_ptr)
{
    size_t size = GET_SIZE(hdr_ptr);
    size_t above_size = GET_SIZE(hdr_ptr - WSIZE);

    // fix header and footer
    set_hdr_ftr(hdr_ptr - above_size, size + above_size, 0);

    // remove below block from free list
    mm_remove(hdr_ptr);
}

/*
 * mm_checkheap - Heap checker
 *
 * memmory validation checker. Checks for loops in the linked list, validates
 * that walking both directions yealds the same list order, possible to walk through
 * each block in physical memory order (slow) and validate every header and footer,
 * validates that all memory addresses are inside the allocated heap
 */
void mm_checkheap(int verbose, int full_check)
{
    size_t numFree1 = 0;
    size_t numFree2 = 0;
    void *bp = prol_ptr;
    size_t memAddr[100000];
    size_t memAddrCounter = 0;

    if (verbose) {
        printf("Heap (%p):\n", prol_ptr);
    }

    // check prologue and epilogue blocks 
    if ((GET_SIZE(prol_ptr) != 0) || !GET_ALLOC(prol_ptr)) {
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
            if (bp < prol_ptr || bp > epil_ptr) {
                printf("%p Memory address lies outside of heap!\n");
            }
        }
    }

    if ((GET_SIZE(epil_ptr) != 0) || !(GET_ALLOC(epil_ptr))) {
        printf("Bad epilogue header\n");
    }

    // print out number of free block in
    if (verbose) {
        printf("%d\n", numFree1);
    }
    
    // check number of free blocks in the explicit list
    // walk list forward
    for(bp = NEXT_BLKP(prol_ptr); GET_SIZE(bp) > 0 && bp != NULL; bp = NEXT_BLKP(bp)) {
        // check if memory address is valid
        if (bp < prol_ptr || bp > epil_ptr) {
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
        if (bp < prol_ptr || bp > epil_ptr) {
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

// prints a single blocks header and footer
// from the function mm-firstfit.c - MODIFIED
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

// checks if a block is 8 byte aligned, allocated bit is the same in
// header and footer and the size is the same in the header and footer
// from the function mm-firstfit.c - MODIFIED
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