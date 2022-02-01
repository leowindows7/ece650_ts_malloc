#ifndef _PROJECT_
#define _PROJECT_
#include <stdio.h>
#include <stdlib.h>

struct _block {
  size_t size;
  struct _block * next;
  struct _block * prev;
};
typedef struct _block block;
/*
 when malloc(either ff_malloc or bf_malloc), the program will firstly search
exiting free space maintained on a linked list, if there is target suitable
for allocating space, the function will use that memory position and update
linked list with necessary operation (add/remove or merge)

implementation details can be found in my_malloc.c

 */
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);
void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);
void * bf_malloc(size_t size, block ** free_list_head, int sbrk_lock);
void bf_free(void * ptr, block ** free_list_head);
unsigned long get_data_segment_size();             // in bytes
unsigned long get_data_segment_free_space_size();  // in bytes
void add_block(block * curr_block);
void remove_block(block * block_toRemove);
void print_blocks();
#endif
