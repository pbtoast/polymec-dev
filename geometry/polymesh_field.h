// Copyright (c) 2012-2018, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef POLYMEC_POLYMESH_FIELD_H
#define POLYMEC_POLYMESH_FIELD_H

#include "core/declare_nd_array.h"
#include "geometry/polymesh.h"

/// \addtogroup geometry geometry
///@{

/// \class polymesh_field
/// This thin wrapper represents a field of values defined on a specific 
/// family of elements on a polymesh.
typedef struct 
{
  /// The underlying polymesh.
  polymesh_t* mesh;

  /// The centering of the field.
  polymesh_centering_t centering;

  /// The number of components for a datum in the field.
  size_t num_components;

  /// The number of locally-stored (possibly multi-component) values in the 
  /// field.
  size_t num_local_values;

  /// The number of (possibly multi-component) ghost values in the field 
  /// (which changes with the number of ghost cells in the mesh).
  size_t num_ghost_values;

  /// Data for the field, and its storage capacity.
  real_t* data;
  size_t capacity;
} polymesh_field_t;

/// Constructs a new polymesh field with the given number of components
/// on the given mesh.
/// \memberof polymesh_field
polymesh_field_t* polymesh_field_new(polymesh_t* mesh,
                                     polymesh_centering_t centering,
                                     size_t num_components);

/// Destroys the given polymesh field.
/// \memberof polymesh_field
void polymesh_field_free(polymesh_field_t* field);

/// Compares all elements in the given component of the two given fields, 
/// returning true if the pairwise comparison of each component element in 
/// the two fields yields a "true" comparison, and false otherwise.
/// Calling this function on two fields with different centerings or with 
/// incompatible meshes is not allowed.
/// \memberof polymesh_field
bool polymesh_field_compare_all(polymesh_field_t* field,
                                polymesh_field_t* other_field,
                                int component,
                                bool (*comparator)(real_t val, real_t other_val));

/// Compares elements in the given component of the two given fields, 
/// returning true if the pairwise comparison of ANY component element in 
/// the two fields yields a "true" comparison, and false if none do so.
/// Calling this function on two fields with different centerings or with 
/// incompatible meshes is not allowed.
/// \memberof polymesh_field
bool polymesh_field_compare_any(polymesh_field_t* field,
                                polymesh_field_t* other_field,
                                int component,
                                bool (*comparator)(real_t val, real_t other_val));

/// Compares elements in the given component of the two given fields, 
/// returning true if NO pairwise comparison of ANY component element in 
/// the two fields yields a "true" comparison, and false if any do.
/// Calling this function on two fields with different centerings or with 
/// incompatible meshes is not allowed.
/// \memberof polymesh_field
bool polymesh_field_compare_none(polymesh_field_t* field,
                                 polymesh_field_t* other_field,
                                 int component,
                                 bool (*comparator)(real_t val, real_t other_val));

///@}

// Defines a 2D array that allows the field to be indexed thus:
// f[i][c] returns the cth component of the value for the ith point.
#define DECLARE_POLYMESH_FIELD_ARRAY(array, field) \
DECLARE_2D_ARRAY(real_t, array, field->data, field->num_local_values + field->num_ghost_values, field->num_components)

#endif

