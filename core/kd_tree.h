#ifndef POLYMEC_KD_TREE_H
#define POLYMEC_KD_TREE_H

#include "core/polymec.h"
#include "core/point.h"
#include "core/slist.h"

// A kd_tree is a collection of points in a 3D domain, stored in a kd-tree 
// so that neighbor searches can be easily and cheaply performed.
typedef struct kd_tree_t kd_tree_t;

// Constructs a kd-tree containing the given points. Data is copied into the tree.
kd_tree_t* kd_tree_new(point_t* points, int num_points);

// Destroys the given tree, freeing its resources.
void kd_tree_free(kd_tree_t* tree);

// Returns the number of points in the tree.
int kd_tree_size(kd_tree_t* tree);

// Returns the index of the point in the tree that is closest to 
// the given point, or -1 if the tree is empty.
int kd_tree_nearest(kd_tree_t* tree, point_t* point);

// Returns a linked list containing the indices of the points in the set 
// found within the given radius of the given point.
int_slist_t* kd_tree_within_radius(kd_tree_t* tree, 
                                   point_t* point, 
                                   double radius);

// This type allows iteration over trees.
typedef struct 
{ 
  void* node; 
} kd_tree_pos_t; 

// Returns a new position/iterator type for iterating over a tree.
kd_tree_pos_t kd_tree_start(kd_tree_t* tree);

// Traverses a kd tree.
bool kd_tree_next(kd_tree_t* tree, kd_tree_pos_t* pos, int* index, double* coords);

#endif

