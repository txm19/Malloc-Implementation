#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)     ((b) + 1)
#define BLOCK_HEADER(ptr) ((struct _block *)(ptr) - 1)

static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes */
   struct _block *next;  /* Pointer to the next _block of allocated memory  */
   bool   free;          /* Is this _block free?                            */
   char   padding[3];    /* Padding: IENTRTMzMjAgU3jMDEED                   */
   struct _block *p_prev;
};


struct _block *heapList = NULL; /* Free list to track the _blocks available */
struct _block *p_lastUsed = NULL;
/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 * \TODO Implement Next Fit
 * \TODO Implement Best Fit
 * \TODO Implement Worst Fit
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;

   if (p_lastUsed == NULL) 
      p_lastUsed = heapList; 

#if defined FIT && FIT == 0
   /* First fit */
   //
   // While we haven't run off the end of the linked list and
   // while the current node we point to isn't free or isn't big enough
   // then continue to iterate over the list.  This loop ends either
   // with curr pointing to NULL, meaning we've run to the end of the list
   // without finding a node or it ends pointing to a free node that has enough
   // space for the request.
   // 
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr = curr->next;
   }
#endif

// \TODO Put your Best Fit code in this #ifdef block
#if defined BEST && BEST == 0
   /** \TODO Implement best fit here */
   struct _block *p_temp = 0;
   int   p_current_optimum_size = 0;

   while (curr != NULL) {
      if ((curr->free) && (curr->size >= size)) {
         if (p_current_optimum_size == 0) {
            p_temp = curr;
            p_current_optimum_size = curr->size;
         } 
         if (curr->size < p_current_optimum_size) {
            p_temp = curr;
            p_current_optimum_size = p_temp->size;
         }

      } // if ((curr->free) && (cur->size >= size)) {
      *last = curr;
      curr = curr->next;		// go next
   } // while (curr != NULL);
   curr = p_temp;
#endif

// \TODO Put your Worst Fit code in this #ifdef block
#if defined WORST && WORST == 0
   /** \TODO Implement worst fit here */
   struct _block *ptr_temp = NULL;
   int p_current_worst_size = 0;

   while(curr != NULL) {
      if (curr->free && curr->size >= size) {
         if (p_current_worst_size < curr->size) {
            ptr_temp = curr;
            p_current_worst_size = ptr_temp->size;
         }
      }
      curr = curr->next;  // go next
   }
   curr = ptr_temp;
#endif

// \TODO Put your Next Fit code in this #ifdef block
#if defined NEXT && NEXT == 0
   /** \TODO Implement next fit here */
   if (curr != NULL)
      curr = p_lastUsed->next;

   while ((curr) && !((curr->free) && ((curr->size) >= size))) {
      *last =  curr;		// assign it
      curr =  curr->next;		// go next
      p_lastUsed = curr;
   }
#endif

   return curr;  // return the address
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   /* Update heapList if not set */
   if (heapList == NULL) 
   {
      heapList = curr;
   }

   /* Attach new _block to previous _block */
   if (last) 
   {
      last->next = curr;
   }

   /* Update _block metadata:
      Set the size of the new block and initialize the new block to "free".
      Set its next pointer to NULL since it's now the tail of the linked list.
   */
   curr->size = size;
   curr->next = NULL;
   curr->free = false;
 
   num_grows = num_grows + 1;
   num_blocks = num_blocks + 1;
   max_heap += size;

  return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{

   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);

   /* Handle 0 size */
   if (size == 0) 
   {
      return NULL;
   }

   /* Look for free _block.  If a free block isn't found then we need to grow our heap. */

   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   /* TODO: If the block found by findFreeBlock is larger than we need then:
            If the leftover space in the new block is greater than the sizeof(_block)+4 then
            split the block.
            If the leftover space in the new block is less than the sizeof(_block)+4 then
            don't split the block.
   */

   if (next != NULL)  // incase for free block
      num_reuses = num_reuses + 1;

   if ((next != NULL) && (((next->size) - size) > (sizeof(struct _block)))) {
      int p_actual_size = 0;
      struct _block *p_actual_next = NULL;
      struct _block *p_new_next = NULL;

      num_blocks = num_blocks + 1;
      num_splits = num_splits + 1;
      p_actual_size = next->size;
      next->size = size;		// get next
      p_actual_next = next->next;
      p_new_next = (struct _block *)(((char *)BLOCK_DATA(next)) + size);  
      next-> next->free = true;      
      next-> next = p_new_next;

      next-> next ->size = p_actual_size - sizeof(struct _block) - size;

      next-> next ->next = p_actual_next;
   }

   /* Could not find free _block, so grow heap */
   if (next == NULL) 
   {
      next = growHeap(last, size);
   }

   /* Could not find free _block or grow heap, so just return NULL */
   if (next == NULL) 
   {
      return NULL;
   }
   
   /* Mark _block as in use */
   next->free = false;

   num_mallocs = num_mallocs + 1;
   num_requested += size;
   
   /* Return data address associated with _block to the user */
   return BLOCK_DATA(next);
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{
   if (ptr == NULL) 
   {
      return;
   }

   /* Make _block as free */
   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;

   /* TODO: Coalesce free _blocks.  If the next block or previous block 
            are free then combine them with this block being freed.
   */
   struct _block *p_check = heapList;
   while (p_check != NULL) {
      if (p_check->next != NULL) {
         if ((p_check->free == true) && (p_check->next->free == true)) {
            struct _block *p_temp_ptr = NULL;

            p_check->free = true;
            p_check->size = p_check->size + sizeof(struct _block) + p_check->next->size;
            p_temp_ptr = p_check->next->next;
            p_check->next = p_temp_ptr;
            num_blocks = num_blocks - 1;
            num_coalesces = num_coalesces + 1;            
         
         } //if ((check->free == true) && (check->next->free == true)) {
      } //if (check->next != NULL) {
      num_frees = num_frees + 1;
   } //while (p_check != NULL) {
}

void *calloc( size_t nmemb, size_t size )
{
   // \TODO Implement calloc
   void *p_ptr = NULL;
   struct _block *tmp = NULL;

   if (size == 0)
      return NULL;
   if (nmemb == 0)
      return NULL;
   
   p_ptr = malloc(nmemb * size);
   if (p_ptr == NULL)
      return NULL;
   
   tmp = BLOCK_HEADER(p_ptr);
   memset(BLOCK_DATA(tmp), 0, tmp->size);
 
   return BLOCK_DATA(tmp); 
}

void *realloc( void *ptr, size_t size )
{
   // \TODO Implement realloc
   struct _block *p_data = NULL;
   struct _block *p_actual = NULL;
   struct _block *p_new = NULL;

   int p_actual_size = 0;

   //zero size? free it
   if (size ==  0) {
      free( ptr ); 
      return NULL;
   }
   if (ptr ==  NULL) 
      return malloc( size);

   p_data = BLOCK_HEADER(ptr);

   if (size > p_data->size) 
      return  malloc(size);
   else {
      num_blocks = num_blocks + 1;
      p_actual_size = p_data->size;
      p_data->size = size;
      p_actual = p_data->next;
      p_new = (struct _block *)(((char *)BLOCK_DATA(p_data)) + size);
      p_data->next = p_new;
      p_data->next->free = true;
      p_data->next->size = p_actual_size - sizeof(struct _block) - size;
      p_data->next->next = p_actual;

      return BLOCK_DATA(p_data);
   }   
   return NULL;
}

/* vim: IENTRTMzMjAgU3ByaW5nIDIwMjM= -----------------------------------------*/
/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/
