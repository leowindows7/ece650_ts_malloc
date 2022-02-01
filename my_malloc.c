#include "my_malloc.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
block * head_block = NULL;
block * free_list_head = NULL;  //lock
__thread block * free_list_head_nolock = NULL;
size_t data_size = 0;
size_t free_space = 0;

void print_blocks() {
  block * curr = free_list_head;
  int i = 0;
  while (curr != NULL) {
    printf("%d(%ld)->", i, curr->size);
    curr = curr->next;
    i += 1;
  }
  printf("NULL\n");
}

// split current free space on linked list and update the list
void * split_block(block * curr_block, size_t size) {
  if (curr_block->size > size + sizeof(block)) {
    block * splitted_block;
    splitted_block = (block *)((char *)curr_block + sizeof(block) + size);
    splitted_block->size = curr_block->size - size - sizeof(block);
    splitted_block->next = NULL;
    splitted_block->prev = NULL;
    remove_block(curr_block);
    add_block(splitted_block);
    curr_block->size = size;
    free_space -= (size + sizeof(block));
  }
  else {
    remove_block(curr_block);
    free_space -= (curr_block->size + sizeof(block));
  }
  curr_block->next = NULL;
  curr_block->prev = NULL;
  return (char *)curr_block + sizeof(block);
}

// add bolock to linked list
void add_block(block * block_toAdd) {
  if (free_list_head == NULL) {  // add when list is empty
    free_list_head = block_toAdd;
    return;
  }
  if (block_toAdd <= free_list_head) {  // add at head
    block_toAdd->prev = NULL;
    block_toAdd->next = free_list_head;
    block_toAdd->next->prev = block_toAdd;  // my last bug was solved here!!
    free_list_head = block_toAdd;
    return;
  }
  block * curr_block = free_list_head;  // add in the middle
  while (curr_block->next != NULL && block_toAdd > curr_block->next) {
    curr_block = curr_block->next;
  }
  if (curr_block->next != NULL) {
    curr_block->next->prev = block_toAdd;
  }
  block_toAdd->next = curr_block->next;
  curr_block->next = block_toAdd;
  block_toAdd->prev = curr_block;
}
// merge two adjacent blocks
void check_merge(block * block_toFree) {
  add_block(block_toFree);
  if (block_toFree->next != NULL) {
    block * block_toFree_ass =
        (block *)((char *)block_toFree + block_toFree->size + sizeof(block));
    if (block_toFree_ass == block_toFree->next) {
      block_toFree->size += block_toFree->next->size + sizeof(block);
      remove_block(block_toFree->next);
    }
  }
  if (block_toFree->prev != NULL) {
    block * block_toFreePre_ass =
        (block *)((char *)block_toFree->prev + block_toFree->prev->size + sizeof(block));
    if (block_toFreePre_ass == block_toFree) {
      block_toFree->prev->size += block_toFree->size + sizeof(block);
      remove_block(block_toFree);
    }
  }
}
void remove_block(block * block_toRemove) {
  if (block_toRemove->prev == NULL &&
      block_toRemove->next != NULL) {  // target  at the beginning of the list
    free_list_head->next->prev = NULL;
    free_list_head = block_toRemove->next;
    block_toRemove->next = NULL;
  }
  else if (block_toRemove->prev != NULL &&
           block_toRemove->next == NULL) {  // target is at the end of the list
    block_toRemove->prev->next = NULL;
    block_toRemove->prev = NULL;
    block_toRemove->next = NULL;
  }
  else if (block_toRemove->prev != NULL &&
           block_toRemove->next != NULL) {  // when target is in the middle
    block_toRemove->prev->next = block_toRemove->next;
    block_toRemove->next->prev = block_toRemove->prev;
    block_toRemove->prev = NULL;
    block_toRemove->next = NULL;
  }
  else if (block_toRemove->prev == NULL && block_toRemove->next == NULL) {
    free_list_head = NULL;  //  remove when there is only one element on the list
  }
}

void bf_free(void * ptr, block ** free_block_head) {  // can be lock / nolock
  block * block_toFree =
      (block *)((char *)ptr - sizeof(block));  // go to where struct pointer is
  free_space += block_toFree->size + sizeof(block);
  // pthread_mutex_lock(&lock);
  check_merge(block_toFree);
  // pthread_mutex_unlock(&lock);
}

//bf malloc is similar to ff other than we will maintain a diff to track the best fit
void * bf_malloc(size_t size, block ** free_block_head, int sbrk_block) {
  block * curr_block = head_block;
  if (curr_block == NULL) {
    data_size += size + sizeof(block);
    block * new_block = sbrk(size + sizeof(block));
    new_block->size = size;
    new_block->prev = NULL;
    new_block->next = NULL;
    head_block = new_block;
    curr_block = new_block;
  }
  else {
    pthread_mutex_lock(&lock);
    size_t diff = INT_MAX;
    block * smallestDiff = NULL;
    //curr_block = free_list_head;
    curr_block = *free_block_head;
    while (curr_block != NULL) {
      if ((curr_block->size >= size && curr_block->size - size < diff)) {
        diff = curr_block->size - size;
        smallestDiff = curr_block;
        if (diff == 0) {
          return split_block(smallestDiff, size);
        }
      }
      curr_block = curr_block->next;
    }
    if (smallestDiff != NULL) {
      return split_block(smallestDiff, size);
    }
    data_size += size + sizeof(block);
    block * new_block =
        sbrk(size + sizeof(block));  // no available space on the list, create a new one
    new_block->size = size;
    new_block->prev = NULL;
    new_block->next = NULL;
    head_block = new_block;
    curr_block = new_block;
    pthread_mutex_unlock(&lock);
  }
  return (char *)curr_block + sizeof(block);
}

void * ts_malloc_lock(size_t size) {
  pthread_mutex_lock(&lock);
  int sbrk_lock = 0;
  void * ans = bf_malloc(size, &free_list_head, sbrk_lock);  // lock
  pthread_mutex_unlock(&lock);
  return ans;
}
void ts_free_lock(void * ptr) {
  pthread_mutex_lock(&lock);
  bf_free(ptr, &free_list_head);
  pthread_mutex_unlock(&lock);
}
void * ts_malloc_nolock(size_t size) {
  int sbrk_lock = 1;
  void * ans = bf_malloc(size, &free_list_head_nolock, sbrk_lock);  // lock
  return ans;
}
void ts_free_nolock(void * ptr) {
  bf_free(ptr, &free_list_head_nolock);
}

unsigned long get_data_segment_size() {
  return data_size;
}

unsigned long get_data_segment_free_space_size() {
  return free_space;
}
