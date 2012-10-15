#ifndef ARBI_ORDERED_MAP_H
#define ARBI_ORDERED_MAP_H

#include "core/avl_tree.h"
#include "core/key_value.h"

// An ordered map is a container that associates keys with values.
// One defines an ordered map and node types using
// DEFINE_ORDERED_MAP(map_name, key_type, value_type, comparator, destructor).

// Interface for a type x_map_t (with node type x_map_node_t 
// and datum x) defined with 
// DEFINE_ORDERED_MAP(x_map, x, x_comparator, x_destructor):
// 
// x_map_t* x_map_new() - Creates a new empty ordered map.
// void x_map_free(x_map_t* map) - Destroys the map.
// void x_map_clear(x_map_t* map) - Empties the map.
// x_map_node_t* x_map_find(x_map_t* map, x_map_key_t key) - Finds the node whose key matches the given one.
// x_map_value_t x_map_value(x_map_t* map, x_map_key_t key) - Finds the value for the given key.
// void x_map_insert(x_map_t* map, x datum) - Inserts a datum into the map.
// void x_map_delete(x_map_t* map, x datum) - Deletes the datum from the map.
// void x_map_foreach(x_map_t* node, x_map_visitor visit, void*) - Executes visit on each map element.

#define DEFINE_ORDERED_MAP(map_name, key_type, value_type, key_comparator, key_destructor, value_destructor) \
DEFINE_KEY_VALUE(map_name##_key_value, key_type, value_type) \
static inline int map_name##_key_value_cmp(map_name##_key_value_t x, map_name##_key_value_t y) \
{ \
  return key_comparator(x.key, y.key); \
} \
static inline void map_name##_key_value_dtor(map_name##_key_value_t x) \
{ \
  static void (*key_dtor)(key_type) = key_destructor; \
  static void (*value_dtor)(value_type) = value_destructor; \
  if (key_dtor != NULL) \
    key_dtor(x.key); \
  if (value_dtor != NULL) \
    value_dtor(x.value); \
} \
DEFINE_AVL_TREE(map_name##_avl_tree, map_name##_key_value_t, map_name##_key_value_cmp, map_name##_key_value_dtor) \
typedef map_name##_avl_tree##_node_t map_name##_node_t; \
typedef key_type map_name##_key_t; \
typedef value_type map_name##_value_t; \
typedef void (*map_name##_visitor)(map_name##_node_t*, void*); \
typedef struct map_name##_t map_name##_t; \
typedef map_name##_avl_tree_destructor map_name##_destructor; \
struct map_name##_t \
{ \
  map_name##_avl_tree_t* tree; \
  int size; \
}; \
\
static inline map_name##_t* map_name##_new() \
{ \
  map_name##_t* map = malloc(sizeof(map_name##_t)); \
  map->tree = map_name##_avl_tree_new(); \
  map->size = 0; \
  return map; \
} \
\
static inline void map_name##_clear(map_name##_t* map) \
{ \
  map_name##_avl_tree_clear(map->tree); \
  map->size = 0; \
} \
\
static inline void map_name##_free(map_name##_t* map) \
{ \
  map_name##_avl_tree_free(map->tree); \
  free(map); \
} \
\
static inline map_name##_node_t* map_name##_find(map_name##_t* map, key_type key) \
{ \
  map_name##_key_value_t kv = {.key = key}; \
  return map_name##_avl_tree_find(map->tree, kv); \
} \
\
static inline map_name##_value_t map_name##_value(map_name##_t* map, key_type key) \
{ \
  map_name##_node_t* node = map_name##_find(map, key); \
  map_name##_key_value_t kv; \
  if (node != NULL) \
  { \
    kv.key = key; \
    kv.value = node->value.value; \
  } \
  return kv.value; \
} \
\
static inline void map_name##_insert(map_name##_t* map, key_type key, value_type value) \
{ \
  map_name##_key_value_t kv = {.key = key, .value = value}; \
  map_name##_avl_tree_insert(map->tree, kv); \
  map->size = map_name##_avl_tree_size(map->tree); \
} \
\
static inline void map_name##_delete(map_name##_t* map, key_type key) \
{ \
  map_name##_node_t* node = map_name##_find(map, key); \
  if (node != NULL) \
  { \
    map_name##_avl_tree_delete(map->tree, node); \
    map->size = map_name##_avl_tree_size(map->tree); \
  } \
} \
\
static inline void map_name##_foreach(map_name##_t* map, map_name##_visitor visit, void* arg) \
{ \
  map_name##_avl_tree_node_visit(map->tree->root, visit, arg); \
} \
\

#endif
