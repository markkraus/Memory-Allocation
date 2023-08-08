
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"


#define UNSCALED_POINTER_ADD(p, x) ((void*)((char*)(p) + (x)))
#define UNSCALED_POINTER_SUB(p, x) ((void*)((char*)(p) - (x)))



typedef struct _BlockInfo {
  // Size of the block and whether or not the block is in use or free.
  // When the size is negative, the block is currently free.
  long int size;
  // Pointer to the previous block in the list.
  struct _Block* prev;
} BlockInfo;

/* A FreeBlockInfo structure contains metadata just for free blocks.
 * When you are ready, you can improve your naive implementation by
 * using these to maintain a separate list of free blocks.
 *
 * These are "kept" in the region of memory that is normally used by
 * the program when the block is allocated. That is, since that space
 * is free anyway, we can make good use of it to improve our malloc.
 */
typedef struct _FreeBlockInfo {
  // Pointer to the next free block in the list.
  struct _Block* nextFree;
  // Pointer to the previous free block in the list.
  struct _Block* prevFree;
} FreeBlockInfo;

/* This is a structure that can serve as all kinds of nodes.
 */
typedef struct _Block {
  BlockInfo info;
  FreeBlockInfo freeNode;
} Block;

/* Pointer to the first FreeBlockInfo in the free list, the list's head. */
static Block* free_list_head = NULL;
static Block* malloc_list_tail = NULL;

static size_t heap_size = 0;

/* Size of a word on this architecture. */
#define WORD_SIZE sizeof(void*)

/* Alignment of blocks returned by mm_malloc.
 * (We need each allocation to at least be big enough for the free space
 * metadata... so let's just align by that.)  */
#define ALIGNMENT (sizeof(FreeBlockInfo))

/* This function will have the OS allocate more space for our heap.
 *
 * It returns a pointer to that new space. That pointer will always be
 * larger than the last request and be continuous in memory.
 */
void* requestMoreSpace(size_t reqSize);

/* This function will get the first block or returns NULL if there is not
 * one.
 *
 * You can use this to start your through search for a block.
 */
Block* first_block();

/* This function will get the adjacent block or returns NULL if there is not
 * one.
 *
 * You can use this to move along your malloc list one block at a time.
 */
Block* next_block(Block* block);

/* Use this function to print a thorough listing of your heap data structures.
 */
void examine_heap();

/* Checks the heap for any issues and prints out errors as it finds them.
 *
 * Use this when you are debugging to check for consistency issues. */
int check_heap();

void removeBlock(Block* block);

void addBlock(Block* block);

Block* searchList(size_t reqSize) {
  Block* ptrFreeBlock = first_block();
  long int checkSize = -reqSize;

  // loop through all free blocks
  while (ptrFreeBlock != NULL) {
    // check if block is large enough
    if (ptrFreeBlock->info.size <= checkSize) {
      // check if block is better than current best block
      return ptrFreeBlock;
    }
    ptrFreeBlock = next_block(ptrFreeBlock);
  }

  // return the best block found to satisy the requested size
  return NULL;
}

/* Find a free block of at least the requested size in the free list.  Returns
   NULL if no free block is large enough. */
Block* searchFreeList(size_t reqSize) {
  Block * ptrFreeBlock = free_list_head;
  long int checkSize = -(reqSize);

  // loop through the free blocks
  while(ptrFreeBlock != NULL){
    // check if free block is large enough
    if(ptrFreeBlock->info.size <= checkSize){
      /* Free block is large enough */
      return ptrFreeBlock;
    }
    // Find the next free block available
    ptrFreeBlock = ptrFreeBlock->freeNode.nextFree;
  }

  // No free block large enough
  return NULL;
}



// TOP-LEVEL ALLOCATOR INTERFACE ------------------------------------

/* Allocate a block of size size and return a pointer to it. If size is zero,
 * returns null.
 */
void* mm_malloc(size_t size) {
  Block* ptrFreeBlock = NULL;
  Block * splitBlock = NULL;
  long int reqSize;

  // Zero-size requests get NULL.
  if (size == 0) {
    return NULL;
  }

  // Determine the amount of memory we want to allocate
  reqSize = size;

  // Round up for correct alignment
  reqSize = ALIGNMENT * ((reqSize + ALIGNMENT - 1) / ALIGNMENT);



  // Find best fit in the FREE LIST
  ptrFreeBlock = searchFreeList(reqSize);


  if (ptrFreeBlock == NULL) {
    // reqSize too big: request more space
    ptrFreeBlock = requestMoreSpace(reqSize + sizeof(BlockInfo));

    // Initialize the new block and add to ALLOCATED LIST
    ptrFreeBlock->info.size = reqSize;
    ptrFreeBlock->info.prev = malloc_list_tail;
    malloc_list_tail = ptrFreeBlock;

  } else {
    // reqSize fits: Add to the ALLOCATED LIST

    // FREE ---> ALLOCATED
    ptrFreeBlock->info.size = -ptrFreeBlock->info.size;

    // Remove from the FREE LIST
    removeBlock(ptrFreeBlock);

  }


  /* SPLIT BLOCK */
   if (ptrFreeBlock->info.size > reqSize + sizeof(BlockInfo)) {

   // Initialize next block of the split block
    Block * nextBlock = next_block(ptrFreeBlock);

   // Compute the address of the new, FREE block
    splitBlock = (Block*) UNSCALED_POINTER_ADD(ptrFreeBlock, reqSize + sizeof(BlockInfo));

    // Set the new size of the FREE block
    splitBlock->info.size = -(ptrFreeBlock->info.size - (reqSize + sizeof(BlockInfo)));

    // Set the size of the ALLOCATED block
    ptrFreeBlock->info.size = reqSize;

    // Link the two split blocks
     splitBlock->info.prev = ptrFreeBlock;


    /* LIST'S TAIL LOCATION */
    if(malloc_list_tail == ptrFreeBlock){
      // split FREE block is the end of the list
      malloc_list_tail = splitBlock;
    } else {
      // split in the list: link split block to rest of list
      nextBlock->info.prev = splitBlock;
    }


    // Add the split, FREE block to the FREE list
    addBlock(splitBlock);
  }

  return UNSCALED_POINTER_ADD(ptrFreeBlock, sizeof(BlockInfo));
}


void coalesce(Block* blockInfo) {

  // Initialize pointers to the sides of block
  Block * nextBlock = next_block(blockInfo);
  Block * previousBlock = blockInfo->info.prev;


  /* NEXT ADJACENT BLOCK */
  if (nextBlock && nextBlock->info.size <= 0) {
    /* Adjacent next block exists and is free */

    // Remove next block from FREE LIST
    removeBlock(nextBlock);

    // Coalesce block with next block
    blockInfo->info.size += nextBlock->info.size - sizeof(BlockInfo);

    if(nextBlock == malloc_list_tail){
      /* Coalescing at the end of the list */

      // block is new tail
      malloc_list_tail = blockInfo;
    } else {
      /* Coalescing in the middle */

      // Move next block up the list
      nextBlock = next_block(nextBlock);

      if(nextBlock){
        /* New next block exists */

        // Link new next block with coalesced block
        nextBlock->info.prev = blockInfo;
      }
    }
  }


  /* PREVIOUS ADJACENT BLOCK*/
  if (previousBlock && previousBlock->info.size <= 0) {
    /* Previous block exists and is free */

    // Update blocks to be coalesced
    nextBlock = blockInfo;
    blockInfo = previousBlock;

    // Remove main block from the FREE LIST
    removeBlock(nextBlock);

    // Coalesce with previous block with main block
    blockInfo->info.size += nextBlock->info.size - sizeof(BlockInfo);

    if(nextBlock == malloc_list_tail){
      /* Coalescing at the end */

      // Coalesced block is new tail
      malloc_list_tail = blockInfo;
    } else {
      /* Coalescing in the middle */

      // Move next block up the list
      nextBlock = next_block(nextBlock);

      if(nextBlock){
        /* New next block exists */

        // Link new next block with coalesced block
        nextBlock->info.prev = blockInfo;
      }
    }
  }
}

/* Free the block referenced by ptr. */
void mm_free(void* ptr) {

  // Get the header information of the block being freed
  Block* blockInfo = (Block*) UNSCALED_POINTER_SUB(ptr, sizeof(BlockInfo));

  // Make the block free
  blockInfo->info.size = -blockInfo->info.size;

  // Add block to FREE LIST
  addBlock(blockInfo);

  // coalesce adjacent free blocks
  coalesce(blockInfo);

}

// PROVIDED FUNCTIONS -----------------------------------------------
//
// You do not need to modify these, but they might be helpful to read
// over.

/* Add a block to the front of the free list */
void addBlock(Block * block){

  if(!block){
    /* Block does not exist */
    return;
  }

  if(free_list_head == NULL){
    /* Free list is empty */

    // New block is head and only member of free list
    block->freeNode.nextFree = NULL;
    block->freeNode.prevFree = NULL;
    free_list_head = block;
  } else {
    /* Free list is NOT empty */

    // Assign new block to front of free list
    free_list_head->freeNode.prevFree = block;
    block->freeNode.nextFree = free_list_head;
    free_list_head = block;
  }
}


/* Take away a block from the free list */
void removeBlock(Block *block) {

  // Pointers to the next and previous free blocks
  Block * next = NULL;
  Block * prev = NULL;

 if(free_list_head == NULL || block == NULL){
  /* Free list is empty */
  return;
 }

  /* POINTER CHECK */
  if(block->freeNode.nextFree){
    /* Free next block exists */
    next = block->freeNode.nextFree;
  }

  if(block->freeNode.prevFree){
    /* Free previous block exists */
    prev = block->freeNode.prevFree;
  }

 if(free_list_head == block) {
  /* Removing head */
    free_list_head = next;
    return;
 }

  if(next){
    /* Free next block exists */

    // Remove a block from the middle of the free list
    next->freeNode.prevFree = prev;
    prev->freeNode.nextFree = next;
  } else {
    /* Free next block does NOT exist */

    // Previous free block is the tail of the free list
    prev->freeNode.nextFree = NULL;
  }
}

/* Get more heap space of exact size reqSize. */
void* requestMoreSpace(size_t reqSize) {
  void* ret = UNSCALED_POINTER_ADD(mem_heap_lo(), heap_size);
  heap_size += reqSize;

  void* mem_sbrk_result = mem_sbrk(reqSize);
  if ((size_t)mem_sbrk_result == -1) {
    printf("ERROR: mem_sbrk failed in requestMoreSpace\n");
    exit(0);
  }

  return ret;
}

/* Initialize the allocator. */
int mm_init() {
  free_list_head = NULL;
  malloc_list_tail = NULL;
  heap_size = 0;

  return 0;
}

/* Gets the first block in the heap or returns NULL if there is not one. */
Block* first_block() {
  Block* first = (Block*)mem_heap_lo();
  if (heap_size == 0) {
    return NULL;
  }

  return first;
}

/* Gets the adjacent block or returns NULL if there is not one. */
Block* next_block(Block* block) {
  size_t distance = (block->info.size > 0) ? block->info.size : -block->info.size;

  Block* end = (Block*)UNSCALED_POINTER_ADD(mem_heap_lo(), heap_size);
  Block* next = (Block*)UNSCALED_POINTER_ADD(block, sizeof(BlockInfo) + distance);
  if (next >= end) {
    return NULL;
  }

  return next;
}

/* Print the heap by iterating through it as an implicit free list. */
void examine_heap() {
  /* print to stderr so output isn't buffered and not output if we crash */
  Block* curr = (Block*)mem_heap_lo();
  Block* end = (Block*)UNSCALED_POINTER_ADD(mem_heap_lo(), heap_size);
  fprintf(stderr, "heap size:\t0x%lx\n", heap_size);
  fprintf(stderr, "heap start:\t%p\n", curr);
  fprintf(stderr, "heap end:\t%p\n", end);

  fprintf(stderr, "free_list_head: %p\n", (void*)free_list_head);

  fprintf(stderr, "malloc_list_tail: %p\n", (void*)malloc_list_tail);

  while(curr && curr < end) {
    /* print out common block attributes */
    fprintf(stderr, "%p: %ld\t", (void*)curr, curr->info.size);

    /* and allocated/free specific data */
    if (curr->info.size > 0) {
      fprintf(stderr, "ALLOCATED\tprev: %p\n", (void*)curr->info.prev);
    } else {
      fprintf(stderr, "FREE\tnextFree: %p, prevFree: %p, prev: %p\n", (void*)curr->freeNode.nextFree, (void*)curr->freeNode.prevFree, (void*)curr->info.prev);
    }

    curr = next_block(curr);
  }
  fprintf(stderr, "END OF HEAP\n\n");

  curr = free_list_head;
  fprintf(stderr, "Head ");
  while(curr) {
    fprintf(stderr, "-> %p ", curr);
    curr = curr->freeNode.nextFree;
  }
  fprintf(stderr, "\n");
}

/* Checks the heap data structure for consistency. */
int check_heap() {
  Block* curr = (Block*)mem_heap_lo();
  Block* end = (Block*)UNSCALED_POINTER_ADD(mem_heap_lo(), heap_size);
  Block* last = NULL;
  long int free_count = 0;

  while(curr && curr < end) {
    if (curr->info.prev != last) {
      fprintf(stderr, "check_heap: Error: previous link not correct.\n");
      examine_heap();
    }

    if (curr->info.size <= 0) {
      // Free
      free_count++;
    }

    last = curr;
    curr = next_block(curr);
  }

  curr = free_list_head;
  last = NULL;
  while(curr) {
    if (curr == last) {
      fprintf(stderr, "check_heap: Error: free list is circular.\n");
      examine_heap();
    }
    last = curr;
    curr = curr->freeNode.nextFree;
    if (free_count == 0) {
      fprintf(stderr, "check_heap: Error: free list has more items than expected.\n");
      examine_heap();
    }
    free_count--;
  }

  return 0;
}
