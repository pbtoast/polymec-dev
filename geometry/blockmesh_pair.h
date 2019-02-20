// Copyright (c) 2012-2019, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef POLYMEC_BLOCKMESH_PAIR_H
#define POLYMEC_BLOCKMESH_PAIR_H

#include "geometry/coord_mapping.h"

/// \addtogroup geometry geometry
///@{

typedef struct blockmesh_t blockmesh_t;
typedef struct unimesh_t unimesh_t;
typedef struct unimesh_patch_t unimesh_patch_t;

/// \struct blockmesh_diffeomorphism_t
/// This struct represents a diffeomorphism that maps quantities from one 
/// block (block1) to another (block2) in a blockmesh. The diffeomorphism is 
/// defined by the coordinate systems for the two blocks and a rotation 
/// in the plane of the boundary connecting them.
struct blockmesh_diffeomorphism_t
{
  /// The coordinate mapping for the first block.
  coord_mapping_t* block1_coords;

  /// The first block boundary.
  unimesh_boundary_t block1_boundary;

  /// The coordinate mapping for the second block.
  coord_mapping_t* block2_coords;

  /// The second block boundary.
  unimesh_boundary_t block2_boundary;

  /// The rotation that must be performed for a quantity to pass through the
  /// face connecting the two blocks. Expressed in units of counterclockwise
  /// revolutions (turns).
  enum
  {
    NO_ROTATION,
    QUARTER_TURN,
    HALF_TURN,
    THREE_QUARTERS_TURN,
    INVALID_ROTATION
  } rotation;
}; 
typedef struct blockmesh_diffeomorphism_t blockmesh_diffeomorphism_t;

/// \class blockmesh_pair
/// This class represents the pairing of two blocks within a blockmesh.
/// \refcounted
typedef struct blockmesh_pair_t blockmesh_pair_t;

/// Returns true if the two given blocks in the given mesh can be connected 
/// using the boundaries defined by arrays of nodes, false if not.
/// \param [in] mesh The blockmesh for the newly-created pair.
/// \param [in] block1_index The index of the first block in the pair (within the mesh).
/// \param [in] block1_nodes An array containing the 4 nodes in the first block
///                          to be identified with the corresponding nodes 
///                          in the second block (block2_nodes).
/// \param [in] block2_index The index of the second block in the pair (within the mesh).
/// \param [in] block2_nodes An array containing the 4 nodes in the second block
///                          to be identified with the corresponding nodes 
///                          in the first block (block1_nodes).
/// \param [out] reason If non-NULL, this string stores an internal string 
///                     that describes any condition preventing a successful
///                     connection between the two blocks.
/// \memberof blockmesh_pair
bool blockmesh_pair_validate(blockmesh_t* mesh, 
                             int block1_index, int block1_nodes[4],
                             int block2_index, int block2_nodes[4],
                             char** reason);

/// Creates a new blockmesh pair for two blocks in the given blockmesh.
/// \param [in] mesh The blockmesh for the newly-created pair.
/// \param [in] block1_index The index of the first block in the pair (within the mesh).
/// \param [in] block1_nodes An array containing the 4 nodes in the first block
///                          to be identified with the corresponding nodes 
///                          in the second block (block2_nodes).
/// \param [in] block2_index The index of the second block in the pair (within the mesh).
/// \param [in] block2_nodes An array containing the 4 nodes in the second block
///                          to be identified with the corresponding nodes 
///                          in the first block (block1_nodes).
/// \returns A newly-created blockmesh pair, or NULL if block1_nodes and block2_nodes
///          don't define a consistent pairing of blocks.
/// \memberof blockmesh_pair
blockmesh_pair_t* blockmesh_pair_new(blockmesh_t* mesh, 
                                     int block1_index, int block1_nodes[4],
                                     int block2_index, int block2_nodes[4]);

/// Returns an internal pointer to a diffeomorphism that maps quantities from the first 
/// block to the second block in this pair. 
/// \memberof blockmesh_pair
blockmesh_diffeomorphism_t* blockmesh_pair_diffeomorphism(blockmesh_pair_t* pair);

/// Determines the patch (i2, j2, k2) within the second block in the pair
/// that corresponds to the patch (i1, j1, k1) in the first block, given the
/// specifics of the connectivity.
/// \param [in] i1 The first logical coordinate for a patch in the first block.
/// \param [in] j1 The second logical coordinate for a patch in the first block.
/// \param [in] k1 The third logical coordinate for a patch in the first block.
/// \param [out] i2 Stores the first logical coordinate for the patch in the second block.
/// \param [out] j2 Stores the second logical coordinate for the patch in the second block.
/// \param [out] k2 Stores the third logical coordinate for the patch in the second block.
/// \memberof blockmesh_pair
void blockmesh_pair_find_patch(blockmesh_pair_t* pair,
                               int i1, int j1, int k1,
                               int* i2, int* j2, int* k2);

/// Returns the number of bytes of data transferred from one patch to another at 
/// the boundary between the two blocks in the pair, given a centering and the 
/// number of (real-valued) components per value.
/// \param [in] centering The centering of the data in question.
/// \param [in] num_comp The number of components for the data in question.
/// \memberof blockmesh_pair
size_t blockmesh_pair_data_size(blockmesh_pair_t* pair,
                                unimesh_centering_t centering,
                                int num_comp);

/// Copies data to a buffer from a source patch in the first block.
/// \param [in] source_patch The patch containing the data to copy to the buffer.
/// \param [out] buffer The buffer to which the patch data is copied.
/// \memberof blockmesh_pair
void blockmesh_pair_copy_in(blockmesh_pair_t* pair,
                            unimesh_patch_t* source_patch,
                            void* buffer);

/// Copies data from a buffer to a destinaton patch in the second block.
/// \param [in] buffer The buffer to which the patch data is copied.
/// \param [out] dest_patch The patch to which data is copied from the buffer.
/// \memberof blockmesh_pair
void blockmesh_pair_copy_out(blockmesh_pair_t* pair,
                             void* buffer,
                             unimesh_patch_t* dest_patch);

///@}

#endif

