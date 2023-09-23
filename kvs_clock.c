#include "kvs_clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct kv_pair {
  char* key;
  char* value;
  int present;
  int modified;
  int reference;
};

struct kvs_clock {
  kvs_base_t* kvs_base;
  int capacity;
  struct kv_pair* cache;
  int current;
};

kvs_clock_t* kvs_clock_new(kvs_base_t* kvs, int capacity) {
  kvs_clock_t* kvs_clock = malloc(sizeof(kvs_clock_t));
  kvs_clock->kvs_base = kvs;
  kvs_clock->capacity = capacity;

  // initializing clock vars
  kvs_clock->current = 0;

  // initializing struct vars
  kvs_clock->cache = malloc(sizeof(struct kv_pair) * capacity);
  for (int i = 0; i < capacity; i++) {
    kvs_clock->cache[i].key = malloc(KVS_KEY_MAX + 1);
    kvs_clock->cache[i].value = malloc(KVS_VALUE_MAX + 1);
    kvs_clock->cache[i].present = 0;
    kvs_clock->cache[i].modified = 0;
    kvs_clock->cache[i].reference = 1;
  }
  return kvs_clock;
}

void kvs_clock_free(kvs_clock_t** ptr) {
  for (int i = 0; i < (*ptr)->capacity; i++) {
    free((*ptr)->cache[i].key);
    free((*ptr)->cache[i].value);
  }
  free((*ptr)->cache);

  free(*ptr);
  *ptr = NULL;
}

int kvs_clock_set(kvs_clock_t* kvs_clock, const char* key, const char* value) {
  if (kvs_clock->capacity == 0) {
    kvs_base_set(kvs_clock->kvs_base, key, value);
    return SUCCESS;
  }

  for (int i = 0; i < kvs_clock->capacity; i++) {
    if (kvs_clock->cache[i].present == 0) {
      strcpy(kvs_clock->cache[i].key, key);
      strcpy(kvs_clock->cache[i].value, value);
      kvs_clock->cache[i].present = 1;
      kvs_clock->cache[i].modified = 1;
      return SUCCESS;
    } else if (strcmp(kvs_clock->cache[i].key, key) == 0) {
      strcpy(kvs_clock->cache[i].value, value);
      kvs_clock->cache[i].reference = 1;
      kvs_clock->cache[i].modified = 1;
      kvs_clock->cache[i].present = 1;
      return SUCCESS;
    }
  }

  // do reference bit algorithm things
  while (kvs_clock->cache[kvs_clock->current % kvs_clock->capacity].reference !=
         0) {
    kvs_clock->cache[kvs_clock->current % kvs_clock->capacity].reference = 0;
    kvs_clock->current++;
  }

  // move hand at the end
  int curr = kvs_clock->current % kvs_clock->capacity;
  if (kvs_clock->cache[curr].modified == 1) {
    kvs_base_set(kvs_clock->kvs_base, kvs_clock->cache[curr].key,
                 kvs_clock->cache[curr].value);
    kvs_clock->cache[curr].modified = 0;
  }
  kvs_clock->cache[curr].modified = 0;
  strcpy(kvs_clock->cache[curr].key, key);
  strcpy(kvs_clock->cache[curr].value, value);
  kvs_clock->cache[curr].modified = 1;
  kvs_clock->cache[curr].reference = 1;
  // kvs_clock->current++;
  return SUCCESS;
}

int kvs_clock_get(kvs_clock_t* kvs_clock, const char* key, char* value) {
  if (kvs_clock->capacity == 0) {
    kvs_base_get(kvs_clock->kvs_base, key, value);
    return SUCCESS;
  }

  // if a cache hits
  for (int i = 0; i < kvs_clock->capacity; i++) {
    if (kvs_clock->cache[i].present == 0) {
      strcpy(kvs_clock->cache[i].key, key);
      kvs_base_get(kvs_clock->kvs_base, key, value);
      strcpy(kvs_clock->cache[i].value, value);
      kvs_clock->cache[i].present = 1;
      return SUCCESS;
    } else if (strcmp(kvs_clock->cache[i].key, key) == 0) {
      strcpy(value, kvs_clock->cache[i].value);
      kvs_clock->cache[i].reference = 1;
      return SUCCESS;
    }
  }

  while (kvs_clock->cache[kvs_clock->current % kvs_clock->capacity].reference !=
         0) {
    kvs_clock->cache[kvs_clock->current % kvs_clock->capacity].reference = 0;
    kvs_clock->current++;
  }

  // if we miss, check the disk
  kvs_base_get(kvs_clock->kvs_base, key, value);
  int curr = kvs_clock->current % kvs_clock->capacity;
  if (kvs_clock->cache[curr].modified == 1) {
    kvs_base_set(
        kvs_clock->kvs_base, kvs_clock->cache[curr].key,
        kvs_clock->cache[curr].value);  // fucks up set if it doesnt exist
    kvs_clock->cache[curr].modified = 0;
  }
  strcpy(kvs_clock->cache[curr].key, key);
  strcpy(kvs_clock->cache[curr].value, value);
  kvs_clock->cache[curr].reference = 1;
  // kvs_clock->current++;
  return SUCCESS;
}

int kvs_clock_flush(kvs_clock_t* kvs_clock) {
  for (int i = 0; i < kvs_clock->capacity; i++) {
    if (kvs_clock->cache[i].modified == 1) {
      kvs_clock->cache[i].modified = 0;
      kvs_base_set(kvs_clock->kvs_base, kvs_clock->cache[i].key,
                   kvs_clock->cache[i].value);
    }
  }
  return SUCCESS;
}
