#include "kvs_lru.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct kv_pair {
  char* key;
  char* value;
  int present;
  int modified;
  int lru;
};

struct kvs_lru {
  kvs_base_t* kvs_base;
  int capacity;
  struct kv_pair* cache;
  int counter;
};

int smallest(kvs_lru_t* kvs_lru) {
  int min = kvs_lru->cache[0].lru;
  int index = 0;
  for (int i = 0; i < kvs_lru->capacity; i++) {
    if (kvs_lru->cache[i].lru < min) {
      min = kvs_lru->cache[i].lru;
      index = i;
    }
  }
  return index;
}

kvs_lru_t* kvs_lru_new(kvs_base_t* kvs, int capacity) {
  kvs_lru_t* kvs_lru = malloc(sizeof(kvs_lru_t));
  kvs_lru->kvs_base = kvs;
  kvs_lru->capacity = capacity;
  kvs_lru->counter = 0;

  kvs_lru->cache = malloc(sizeof(struct kv_pair) * capacity);
  for (int i = 0; i < capacity; i++) {
    kvs_lru->cache[i].key = malloc(KVS_KEY_MAX + 1);
    kvs_lru->cache[i].value = malloc(KVS_VALUE_MAX + 1);
    kvs_lru->cache[i].present = 0;
    kvs_lru->cache[i].modified = 0;
    kvs_lru->cache[i].lru = 0;
  }
  return kvs_lru;
}

void kvs_lru_free(kvs_lru_t** ptr) {
  for (int i = 0; i < (*ptr)->capacity; i++) {
    free((*ptr)->cache[i].key);
    free((*ptr)->cache[i].value);
  }
  free((*ptr)->cache);

  free(*ptr);
  *ptr = NULL;
}

int kvs_lru_set(kvs_lru_t* kvs_lru, const char* key, const char* value) {
  // if capacity 0
  if (kvs_lru->capacity == 0) {
    kvs_base_set(kvs_lru->kvs_base, key, value);
    return SUCCESS;
  }

  for (int i = 0; i < kvs_lru->capacity; i++) {
    if (kvs_lru->cache[i].present == 0) {
      strcpy(kvs_lru->cache[i].key, key);
      strcpy(kvs_lru->cache[i].value, value);
      kvs_lru->cache[i].lru = kvs_lru->counter;
      kvs_lru->counter++;
      kvs_lru->cache[i].present = 1;
      kvs_lru->cache[i].modified = 1;
      return SUCCESS;
    } else if (strcmp(kvs_lru->cache[i].key, key) == 0) {
      strcpy(kvs_lru->cache[i].value, value);
      kvs_lru->cache[i].lru = kvs_lru->counter;
      kvs_lru->counter++;
      kvs_lru->cache[i].modified = 1;
      return SUCCESS;
    }
  }

  int replace = smallest(kvs_lru);
  if (kvs_lru->cache[replace].modified == 1) {
    kvs_base_set(kvs_lru->kvs_base, kvs_lru->cache[replace].key,
                 kvs_lru->cache[replace].value);
  }
  strcpy(kvs_lru->cache[replace].key, key);
  strcpy(kvs_lru->cache[replace].value, value);
  kvs_lru->cache[replace].lru = kvs_lru->counter;
  kvs_lru->counter++;
  kvs_lru->cache[replace].modified = 1;
  return SUCCESS;
}

int kvs_lru_get(kvs_lru_t* kvs_lru, const char* key, char* value) {
  if (kvs_lru->capacity == 0) {
    kvs_base_get(kvs_lru->kvs_base, key, value);
    return SUCCESS;
  }

  for (int i = 0; i < kvs_lru->capacity; i++) {
    if (kvs_lru->cache[i].present == 0) {
      strcpy(kvs_lru->cache[i].key, key);
      kvs_base_get(kvs_lru->kvs_base, key, value);
      strcpy(kvs_lru->cache[i].value, value);
      kvs_lru->cache[i].lru = kvs_lru->counter;
      kvs_lru->counter++;
      kvs_lru->cache[i].present = 1;
      return SUCCESS;
    } else if (strcmp(kvs_lru->cache[i].key, key) == 0) {
      strcpy(value, kvs_lru->cache[i].value);
      kvs_lru->cache[i].lru = kvs_lru->counter;
      kvs_lru->counter++;
      return SUCCESS;
    }
  }

  kvs_base_get(kvs_lru->kvs_base, key, value);

  // needs work here
  int replace = smallest(kvs_lru);
  if (kvs_lru->cache[replace].modified == 1) {
    kvs_base_set(kvs_lru->kvs_base, kvs_lru->cache[replace].key,
                 kvs_lru->cache[replace].value);
  }
  strcpy(kvs_lru->cache[replace].key, key);
  strcpy(kvs_lru->cache[replace].value, value);
  kvs_lru->cache[replace].lru = kvs_lru->counter;
  kvs_lru->counter++;
  kvs_lru->cache[replace].modified = 0;
  return SUCCESS;
}

int kvs_lru_flush(kvs_lru_t* kvs_lru) {
  for (int i = 0; i < kvs_lru->capacity; i++) {
    if (kvs_lru->cache[i].modified == 1) {
      kvs_lru->cache[i].modified = 0;
      kvs_base_set(kvs_lru->kvs_base, kvs_lru->cache[i].key,
                   kvs_lru->cache[i].value);
    }
  }

  return SUCCESS;
}
