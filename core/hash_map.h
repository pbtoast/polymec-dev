// Copyright (c) 2012-2014, Jeffrey N. Johnson
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this 
// list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef POLYMEC_HASH_MAP_H
#define POLYMEC_HASH_MAP_H

#include <stdlib.h>
#include "core/polymec.h"
#include "core/hash_functions.h"
#include "core/comparators.h"

// A hash map is a hash table that associates keys with values.
// One defines a hash map using
// DEFINE_HASH_MAP(map_name, key_type, value_type, hash_func).

// Interface for a type x_map_t (with key type K and 
// value type V) defined with 
// DEFINE_HASH_MAP(x_map, K, V, x_hash, key_equals):
// 
// x_map_t* x_map_new() - Creates a new empty hash map.
// x_map_t* x_map_new_with_capacity(int N) - Creates a new empty hash map with 
//                                           initial capacity N.
// void x_map_free(x_map_t* map) - Destroys the map.
// void x_map_clear(x_map_t* map) - Empties the map.
// x_map_value_t* x_map_get(x_map_t* map, x_map_key_t key) - Returns the value for the key, or NULL.
// bool x_map_contains(x_map_t* map, x_map_key_t key) - Returns true if the map contains the key, false if not.
// void x_map_insert(x_map_t* map, x_map_key_t key, x_map_value_t value) - Sets the value for the given key.
// void x_map_delete(x_map_t* map, x_map_key_t key) - Deletes the value for the given key.
// void x_map_foreach(x_map_t* node, x_map_visitor visit, void*) - Executes visit on each map element.

#define DEFINE_HASH_MAP(map_name, key_type, value_type, hash_func, equals_func) \
typedef key_type map_name##_key_t; \
typedef value_type map_name##_value_t; \
typedef int (*map_name##_hash_func)(map_name##_key_t); \
typedef bool (*map_name##_equals_func)(map_name##_key_t, map_name##_key_t); \
typedef void (*map_name##_visitor)(map_name##_key_t, map_name##_value_t, void*); \
typedef struct map_name##_entry_t map_name##_entry_t; \
struct map_name##_entry_t \
{ \
  key_type key; \
  int hash; \
  value_type value; \
  map_name##_entry_t* next; \
}; \
\
typedef struct \
{ \
  map_name##_entry_t** buckets; \
  int bucket_count; \
  map_name##_hash_func hash; \
  map_name##_equals_func equals; \
  int size; \
} map_name##_t; \
\
static inline map_name##_t* map_name##_new_with_capacity(int N) \
{ \
  map_name##_t* map = polymec_malloc(sizeof(map_name##_t)); \
  int minimum_bucket_count = N * 4 / 3; \
  map->bucket_count = 1; \
  while (map->bucket_count <= minimum_bucket_count) \
    map->bucket_count <<= 1; \
  map->buckets = polymec_malloc(map->bucket_count * sizeof(map_name##_entry_t*)); \
  memset(map->buckets, 0, map->bucket_count * sizeof(map_name##_entry_t*)); \
  ASSERT(map->buckets != NULL); \
  map->size = 0; \
  map->hash = hash_func; \
  map->equals = equals_func; \
  return map; \
} \
\
static inline map_name##_t* map_name##_new() \
{ \
  return map_name##_new_with_capacity(32); \
} \
\
static inline void map_name##_clear(map_name##_t* map) \
{ \
  for (int i = 0; i < map->bucket_count; ++i) \
  { \
    map_name##_entry_t* entry = map->buckets[i]; \
    while (entry != NULL) \
    { \
      map_name##_entry_t* next = entry->next; \
      polymec_free(entry); \
      entry = next; \
    } \
    map->buckets[i] = NULL; \
  } \
  map->size = 0; \
} \
\
static inline void map_name##_free(map_name##_t* map) \
{ \
  map_name##_clear(map); \
  polymec_free(map->buckets); \
  polymec_free(map); \
} \
\
static inline int map_name##_hash(map_name##_t* map, key_type key) \
{ \
  int h = map->hash(key); \
  h += ~(h << 9); \
  h ^= (((unsigned int) h) >> 14); \
  h += (h << 4); \
  h ^= (((unsigned int) h) >> 10); \
  return h; \
} \
\
static inline int map_name##_index(int bucket_count, int hash) \
{ \
  return hash & (bucket_count - 1); \
} \
\
static inline bool map_name##_keys_equal(map_name##_t* map, key_type key1, int hash1, key_type key2, int hash2) \
{ \
  return ((hash1 == hash2) && map->equals(key1, key2)); \
} \
\
static inline map_name##_value_t* map_name##_get(map_name##_t* map, key_type key) \
{ \
  int h = map_name##_hash(map, key); \
  int index = map_name##_index(map->bucket_count, h); \
  map_name##_entry_t* entry = map->buckets[index]; \
  while (entry != NULL) \
  { \
    if (map_name##_keys_equal(map, entry->key, entry->hash, key, h)) \
      return &(entry->value); \
    entry = entry->next; \
  } \
  return NULL; \
} \
\
static inline bool map_name##_contains(map_name##_t* map, key_type key) \
{ \
  int h = map_name##_hash(map, key); \
  int index = map_name##_index(map->bucket_count, h); \
  map_name##_entry_t* entry = map->buckets[index]; \
  while (entry != NULL) \
  { \
    if (map_name##_keys_equal(map, entry->key, entry->hash, key, h)) \
      return true; \
    entry = entry->next; \
  } \
  return false; \
} \
\
static inline void map_name##_expand(map_name##_t* map) \
{ \
  if (map->size > (map->bucket_count * 3/4)) \
  { \
    int new_count = map->bucket_count * 2; \
    map_name##_entry_t** new_buckets = polymec_malloc(new_count * sizeof(map_name##_entry_t*)); \
    memset(new_buckets, 0, new_count * sizeof(map_name##_entry_t*)); \
    if (new_buckets == NULL) \
      return; \
    for (int i = 0; i < map->bucket_count; ++i) \
    { \
      map_name##_entry_t* entry = map->buckets[i]; \
      while (entry != NULL) \
      { \
        map_name##_entry_t* next = entry->next; \
        int index = map_name##_index(new_count, entry->hash); \
        entry->next = new_buckets[index]; \
        new_buckets[index] = entry; \
        entry = next; \
      } \
    } \
    polymec_free(map->buckets); \
    map->buckets = new_buckets; \
    map->bucket_count = new_count; \
  } \
} \
\
static inline void map_name##_insert(map_name##_t* map, key_type key, value_type value) \
{ \
  int h = map_name##_hash(map, key); \
  int index = map_name##_index(map->bucket_count, h); \
  map_name##_entry_t** p = &(map->buckets[index]); \
  while (true) \
  { \
    map_name##_entry_t* current = *p; \
    if (current == NULL) \
    { \
      *p = polymec_malloc(sizeof(map_name##_entry_t)); \
      (*p)->key = key; \
      (*p)->hash = h; \
      (*p)->value = value; \
      (*p)->next = NULL; \
      map->size++; \
      map_name##_expand(map); \
      return; \
    } \
    if (map_name##_keys_equal(map, current->key, current->hash, key, h)) \
      current->value = value; \
    p = &current->next; \
  } \
} \
\
static inline void map_name##_delete(map_name##_t* map, key_type key) \
{ \
  int h = map_name##_hash(map, key); \
  int index = map_name##_index(map->bucket_count, h); \
  map_name##_entry_t** p = &(map->buckets[index]); \
  map_name##_entry_t* current; \
  while ((current = *p) != NULL) \
  { \
    if (map_name##_keys_equal(map, current->key, current->hash, key, h)) \
    { \
      *p = current->next; \
      polymec_free(current); \
      map->size--; \
    }\
    else \
      p = &current->next; \
  } \
} \
\
static inline void map_name##_foreach(map_name##_t* map, map_name##_visitor visit, void* arg) \
{ \
  for (int i = 0; i < map->bucket_count; ++i) \
  { \
    map_name##_entry_t* entry = map->buckets[i]; \
    while (entry != NULL) \
    { \
      visit(entry->key, entry->value, arg); \
      entry = entry->next; \
    } \
  } \
} \
\

// Define some hash maps.
DEFINE_HASH_MAP(str_ptr_hash_map, char*, void*, string_hash, string_equals)

#endif
