#include "kvs_fifo.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct kv_pair {
  char* key;
  char* value;
  int present;
  int modified;
};

struct kvs_fifo {
  kvs_base_t* kvs_base;
  int capacity;
  struct kv_pair* cache;
  int head;
};

// how to create a cache???
kvs_fifo_t* kvs_fifo_new(kvs_base_t* kvs, int capacity) {
  kvs_fifo_t* kvs_fifo = malloc(sizeof(kvs_fifo_t));
  kvs_fifo->kvs_base = kvs;
  kvs_fifo->capacity = capacity;
  kvs_fifo->head = 0;
  // create a cache that is capacity size
  kvs_fifo->cache = malloc(sizeof(struct kv_pair) * capacity);
  for (int i = 0; i < capacity; i++) {
    kvs_fifo->cache[i].key = malloc(KVS_KEY_MAX + 1);
    kvs_fifo->cache[i].value = malloc(KVS_VALUE_MAX + 1);
    kvs_fifo->cache[i].present = 0;
    kvs_fifo->cache[i].modified = 0;
  }
  return kvs_fifo;
}

void kvs_fifo_free(kvs_fifo_t** ptr) {
  for (int i = 0; i < (*ptr)->capacity; i++) {
    free((*ptr)->cache[i].key);
    free((*ptr)->cache[i].value);
  }
  free((*ptr)->cache);
  free(*ptr);
  *ptr = NULL;
}

int kvs_fifo_set(kvs_fifo_t* kvs_fifo, const char* key, const char* value) {
  if (kvs_fifo->capacity == 0) {
    kvs_base_set(kvs_fifo->kvs_base, key, value);
    return SUCCESS;
  }

  for (int i = 0; i < kvs_fifo->capacity; i++) {
    if (kvs_fifo->cache[i].present == 0) {  // if there is an empty key
      strcpy(kvs_fifo->cache[i].key, key);
      strcpy(kvs_fifo->cache[i].value, value);
      kvs_fifo->cache[i].present = 1;
      kvs_fifo->cache[i].modified = 1;
      return SUCCESS;
    } else if (strcmp(kvs_fifo->cache[i].key, key) == 0) {  // if the key exists
      strcpy(kvs_fifo->cache[i].value, value);
      kvs_fifo->cache[i].modified = 1;
      return SUCCESS;
    }
  }
  // if it doesnt already exist and cache is full:

  // sets the head index into disk memory, evicting it
  if (kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].modified == 1) {
    kvs_base_set(kvs_fifo->kvs_base,
                 kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].key,
                 kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].value);
    kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].modified = 0;
  }
  // copies the values of the new set into the cache on index head
  strcpy(kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].key, key);
  strcpy(kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].value, value);
  kvs_fifo->cache[kvs_fifo->head % kvs_fifo->capacity].modified = 1;
  // increment head
  kvs_fifo->head++;
  return SUCCESS;
}

int kvs_fifo_get(kvs_fifo_t* kvs_fifo, const char* key, char* value) {
  if (kvs_fifo->capacity == 0) {
    kvs_base_get(kvs_fifo->kvs_base, key, value);
    return SUCCESS;
  }

  // checks if value is in cache
  for (int i = 0; i < kvs_fifo->capacity; i++) {
    if (kvs_fifo->cache[i].present == 0) {
      strcpy(kvs_fifo->cache[i].key, key);
      kvs_base_get(kvs_fifo->kvs_base, key, value);
      strcpy(kvs_fifo->cache[i].value, value);
      kvs_fifo->cache[i].present = 1;
      return SUCCESS;
    } else if (strcmp(kvs_fifo->cache[i].key, key) == 0) {
      strcpy(value, kvs_fifo->cache[i].value);
      return SUCCESS;
    }
  }

  // if not, we have to check if it is in the disk
  kvs_base_get(kvs_fifo->kvs_base, key, value);
  // lastly, we have to cache the new pair into memory, potentially evicting
  // another entry
  int curr = kvs_fifo->head % kvs_fifo->capacity;
  if (kvs_fifo->cache[curr].modified == 1) {
    kvs_base_set(kvs_fifo->kvs_base, kvs_fifo->cache[curr].key,
                 kvs_fifo->cache[curr].value);
    kvs_fifo->cache[curr].modified = 0;
  }
  // copies the values of the new set into the cache on index head
  strcpy(kvs_fifo->cache[curr].key, key);
  strcpy(kvs_fifo->cache[curr].value, value);
  kvs_fifo->head++;
  return SUCCESS;
}

int kvs_fifo_flush(kvs_fifo_t* kvs_fifo) {
  for (int i = 0; i < kvs_fifo->capacity; i++) {
    if (kvs_fifo->cache[i].modified == 1) {
      kvs_fifo->cache[i].modified = 0;
      kvs_base_set(kvs_fifo->kvs_base, kvs_fifo->cache[i].key,
                   kvs_fifo->cache[i].value);
    }
  }

  return SUCCESS;
}
