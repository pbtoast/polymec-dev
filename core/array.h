#ifndef POLYMEC_ARRAY_H
#define POLYMEC_ARRAY_H

#include "core/polymec.h"
#include "arena/proto.h"

// An array is a dynamically-resizable chunk of contiguous memory that 
// stores a particular data type. It serves the same purpose as a C++ vector.
// One defines an array using
// DEFINE_ARRAY(array_name, element)
//
// Interface for a type x_array_t (with datum x) defined with 
// DEFINE_ARRAY(x_array, x):
//
// x_array_t* x_array_new() - Creates a new, empty array on the heap.
// x_array_t* x_array_new_with_capacity(int capacity) - Creates a new, empty array on the heap with the given capacity.
// x_array_t* x_array_new_with_arena(ARENA* arena) - Creates a new, empty array using the given arena.
// x_array_t* x_array_new_with_arena_and_capacity(ARENA* arena, int capacity) - Creates a new, empty array using the given arena with the given capacity.
// x_array_t empty_x_array() - Creates a new, empty array on the stack.
// void x_array_free(array_t* array) - Destroys the (heap-allocated) array.
// x* x_array_find(x_array_t* array, x value, cmp_func comparator) - Performs a linear search within the array, returning the pointer to the found item or NULL if not found.
// void x_sarray_append(x_array_t* array, x value) - Appends an x to the end of the array.
// void x_sarray_append_with_dtor(x_array_t* array, x value, destructor dtor) - Appends an x to the end of the array, using dtor to destroy when finished.
// bool x_sarray_empty(x_array_t* array) - Returns true if empty, false otherwise.
// void x_sarray_clear(x_array_t* array) - Clears the given array, making it empty.
// void x_sarray_resize(x_array_t* array, int new_size) - Resizes the array, keeping data intact if possible.
// void x_sarray_reserve(x_array_t* array, int new_capacity) - Reserves storage for the given capacity within the array. No effect if the array already has sufficient storage.
// 
// Member data for an array a:
// 
// a.data - A pointer to an array containing the actual data.
// a.dtors - A pointer to an array of destructors for data elements.
// a.size - Number of elements in the array.
// a.capacity - Number of elements in the array.

#define DEFINE_ARRAY(array_name, element) \
typedef void (*array_name##_dtor)(element); \
typedef struct \
{ \
  element* data; \
  array_name##_dtor* dtors; \
  int size; \
  int capacity; \
  ARENA* arena; \
} array_name##_t; \
\
typedef int (*array_name##_comparator)(element, element); \
\
static inline void array_name##_reserve(array_name##_t* array, int new_capacity) \
{ \
  ASSERT(new_capacity > 0); \
  if (new_capacity > array->capacity) \
  { \
    array->data = ARENA_REALLOC(array->arena, array->data, sizeof(element) * new_capacity, 0); \
    array->capacity = new_capacity; \
  } \
} \
\
static inline array_name##_t* array_name##_new_with_arena_and_capacity(ARENA* arena, int capacity) \
{ \
  ASSERT(capacity > 0); \
  array_name##_t* array = ARENA_MALLOC(arena, sizeof(array_name##_t), 0); \
  array->data = NULL; \
  array->dtors = NULL; \
  array->size = 0; \
  array->capacity = 0; \
  array->arena = arena; \
  array_name##_reserve(array, capacity); \
  return array; \
} \
\
static inline array_name##_t* array_name##_new_with_arena(ARENA* arena) \
{ \
  return array_name##_new_with_arena_and_capacity(arena, 16); \
} \
\
static inline array_name##_t* array_name##_new_with_capacity(int capacity) \
{ \
  return array_name##_new_with_arena_and_capacity(NULL, capacity); \
} \
\
static inline array_name##_t* array_name##_new() \
{ \
  return array_name##_new_with_capacity(16); \
} \
\
static inline array_name##_t empty_##array_name() \
{ \
  static array_name##_t empty = {.data = NULL, .size = 0, .capacity = 0}; \
  return empty; \
} \
\
static inline void array_name##_resize(array_name##_t* array, int new_size) \
{ \
  ASSERT(new_size >= 0); \
  if (new_size > array->capacity) \
  { \
    while (array->capacity < new_size) \
      array_name##_reserve(array, MAX(16, 2 * array->capacity)); \
  } \
  else if (new_size < array->size) \
  { \
    if (array->dtors != NULL) \
    { \
      for (int i = 0; i < array->size; ++i) \
      { \
        if (array->dtors[i] != NULL) \
          array->dtors[i](array->data[i]); \
      } \
    } \
  } \
  array->size = new_size; \
} \
\
static inline void array_name##_clear(array_name##_t* array) \
{ \
  array_name##_resize(array, 0); \
} \
\
static inline void array_name##_free(array_name##_t* array) \
{ \
  array_name##_clear(array); \
  if (array->dtors != NULL) \
    free(array->dtors); \
  if (array->data != NULL) \
    free(array->data); \
  free(array); \
} \
\
static inline bool array_name##_empty(array_name##_t* array) \
{ \
  return (array->size == 0); \
} \
\
static inline element* array_name##_find(array_name##_t* array, element value, array_name##_comparator comparator) \
{ \
  for (int i = 0; i < array->size; ++i) \
  { \
    if (comparator(value, array->data[i]) == 0) \
      return &array->data[i]; \
  } \
  return NULL; \
} \
static inline void array_name##_append_with_dtor(array_name##_t* array, element value, array_name##_dtor dtor) \
{ \
  array_name##_resize(array, array->size + 1); \
  array->data[array->size-1] = value; \
  if (dtor != NULL) \
  { \
    array->dtors = ARENA_REALLOC(array->arena, array->dtors, sizeof(array_name##_dtor) * array->capacity, 0); \
    array->dtors[array->size-1] = dtor; \
  } \
} \
\
static inline void array_name##_append(array_name##_t* array, element value) \
{ \
  array_name##_append_with_dtor(array, value, NULL); \
} \

// Define some basic sarray types.
DEFINE_ARRAY(int_array, int)
DEFINE_ARRAY(double_array, double)
DEFINE_ARRAY(str_array, char*)
DEFINE_ARRAY(ptr_array, void*)

#endif
