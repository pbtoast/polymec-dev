// Copyright (c) 2012-2019, Jeffrey N. Johnson
// All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef POLYMEC_TENSOR2_H
#define POLYMEC_TENSOR2_H

#include "core/polymec.h"
#include "core/point.h"

/// \addtogroup core core
///@{

/// \class tensor2
/// A rank-2 general (non-symmetric) tensor in 3D space. You can cast this to
/// and from an array of 9 real_t. It is stored in column-major order so that
/// you can cast it to and from a 3x3 matrix.
typedef struct
{
  real_t xx, yx, zx,
         xy, yy, zy,
         xz, yz, zz;
} tensor2_t;

/// Allocates a new tensor2 on the heap with the given components. Not
/// necessary if you are allocating a tensor2 on the stack.
/// \memberof tensor2
/// \refcounted
tensor2_t* tensor2_new(real_t xx, real_t xy, real_t xz,
                       real_t yx, real_t yy, real_t yz,
                       real_t zx, real_t zy, real_t zz);

/// Copies tensor components from src to dest.
/// \memberof tensor2
static inline void tensor2_copy(tensor2_t* src, tensor2_t* dest)
{
  memcpy(dest, src, 9*sizeof(real_t));
}

/// Sets the components of the given tensor.
/// \memberof tensor2
static inline void tensor2_set(tensor2_t* t,
                               real_t xx, real_t xy, real_t xz,
                               real_t yx, real_t yy, real_t yz,
                               real_t zx, real_t zy, real_t zz)
{
  t->xx = xx; t->xy = xy; t->xz = xz;
  t->yx = yx; t->yy = yy; t->yz = yz;
  t->zx = zx; t->zy = zy; t->zz = zz;
}

/// Sets the the given tensor to the 3x3 identity tensor, scaled by the
/// given factor.
/// \memberof tensor2
static inline void tensor2_set_identity(tensor2_t* t, real_t factor)
{
  t->xx = factor; t->xy = 0.0;    t->xz = 0.0;
  t->yx = 0.0;    t->yy = factor; t->yz = 0.0;
  t->zx = 0.0;    t->zy = 0.0;    t->zz = factor;
}

/// Scales the components of the given tensor.
/// \memberof tensor2
static inline void tensor2_scale(tensor2_t* t, real_t factor)
{
  t->xx *= factor; t->xy *= factor; t->xz *= factor;
  t->yx *= factor; t->yy *= factor; t->yz *= factor;
  t->zx *= factor; t->zy *= factor; t->zz *= factor;
}

/// Returns the determinant of the tensor.
/// \memberof tensor2
static inline real_t tensor2_det(tensor2_t* t)
{
  return t->xx * (t->yy*t->zz - t->zy*t->yz) -
         t->xy * (t->yx*t->zz - t->zx*t->yz) +
         t->xz * (t->yx*t->zy - t->zx*t->yy);
}

/// Returns the trace of the tensor.
/// \memberof tensor2
static inline real_t tensor2_trace(tensor2_t* t)
{
  return t->xx + t->yy + t->zz;
}

/// Computes the (vector-valued) tensor-vector product.
/// \memberof tensor2
static inline void tensor2_dot_vector(tensor2_t* t, vector_t* v, vector_t* tv)
{
  tv->x = t->xx*v->x + t->xy*v->y + t->xz*v->z;
  tv->y = t->yx*v->x + t->yy*v->y + t->yz*v->z;
  tv->z = t->zx*v->x + t->zy*v->y + t->zz*v->z;
}

/// Computes the (vector-valued) transposed tensor-vector product.
/// \memberof tensor2
static inline void tensor2_dot_vector_t(tensor2_t* t, vector_t* v, vector_t* tv)
{
  tv->x = t->xx*v->x + t->yx*v->y + t->zx*v->z;
  tv->y = t->xy*v->x + t->yy*v->y + t->zy*v->z;
  tv->z = t->xz*v->x + t->yz*v->y + t->zz*v->z;
}

/// Computes the double dot product x dot t dot y.
/// \memberof tensor2
static inline real_t tensor2_ddot(tensor2_t* t, vector_t* x, vector_t* y)
{
  vector_t ty;
  tensor2_dot_vector(t, y, &ty);
  return vector_dot(x, &ty);
}

/// Computes the inverse of the tensor.
/// \memberof tensor2
static inline void tensor2_invert(tensor2_t* t, tensor2_t* t_inverse)
{
#ifndef NDEBUG
  real_t det_t = tensor2_det(t);
  ASSERT(!reals_equal(det_t, 0.0));
#endif
  t_inverse->xx = t->yy*t->zz - t->yz*t->zy;
  t_inverse->xy = t->xz*t->zy - t->xy*t->zz;
  t_inverse->xz = t->xy*t->yz - t->xz*t->yy;
  t_inverse->yx = t->yz*t->zx - t->yx*t->zz;
  t_inverse->yy = t->xx*t->zz - t->xz*t->zx;
  t_inverse->yz = t->xz*t->yx - t->xx*t->yz;
  t_inverse->zx = t->yx*t->zy - t->yy*t->zx;
  t_inverse->zy = t->xy*t->zx - t->xx*t->zy;
  t_inverse->zz = t->xx*t->yy - t->xy*t->yx;
}

/// Writes a text representation of the tensor to the given stream.
/// \memberof tensor2
void tensor2_fprintf(tensor2_t* t, FILE* stream);

/// \class symtensor2
/// A rank-2 symmetric tensor in 3D space. You can cast this to and from an
/// array of 6 real_t. Not castable to any Fortran matrix type.
typedef struct
{
  real_t xx, xy, xz,
             yy, yz,
                 zz;
} symtensor2_t;

/// Allocates a new symmetric tensor2 on the heap with the given components. Not
/// necessary if you are allocating a symmetric tensor2 on the stack.
/// \memberof symtensor2
/// \refcounted
symtensor2_t* symtensor2_new(real_t xx, real_t xy, real_t xz,
                                        real_t yy, real_t yz,
                                                   real_t zz);

/// Copies symmetric tensor components from src to dest.
/// \memberof symtensor2
static inline void symtensor2_copy(symtensor2_t* src, symtensor2_t* dest)
{
  memcpy(dest, src, 6*sizeof(real_t));
}

/// Sets the components of the given symmetric tensor.
/// \memberof symtensor2
static inline void symtensor2_set(symtensor2_t* t,
                                  real_t xx, real_t xy, real_t xz,
                                             real_t yy, real_t yz,
                                                        real_t zz)
{
  t->xx = xx; t->xy = xy; t->xz = xz;
              t->yy = yy; t->yz = yz;
                          t->zz = zz;
}

/// Sets the given symmetric tensor to a 3x3 identity tensor scaled by
/// the given factor.
/// \memberof symtensor2
static inline void symtensor2_set_identity(symtensor2_t* t, real_t factor)
{
  t->xx = factor; t->xy = 0.0;    t->xz = 0.0;
                  t->yy = factor; t->yz = 0.0;
                                  t->zz = factor;
}

/// Scales the components of the given symmetric tensor.
/// \memberof symtensor2
static inline void symtensor2_scale(symtensor2_t* t, real_t factor)
{
  t->xx *= factor; t->xy *= factor; t->xz *= factor;
                   t->yy *= factor; t->yz *= factor;
                                    t->zz *= factor;
}

/// Returns the determinant of the symmetric tensor.
/// \memberof symtensor2
static inline real_t symtensor2_det(symtensor2_t* t)
{
  return t->xx * (t->yy*t->zz - t->yz*t->yz) -
         t->xy * (t->xy*t->zz - t->xz*t->yz) +
         t->xz * (t->xy*t->yz - t->xz*t->yy);
}

/// Returns the trace of the symmetric tensor.
/// \memberof symtensor2
static inline real_t symtensor2_trace(symtensor2_t* t)
{
  return t->xx + t->yy + t->zz;
}

/// Computes the (vector-valued) symmetric-tensor-vector product.
/// \memberof symtensor2
static inline void symtensor2_dot_vector(symtensor2_t* t, vector_t* v, vector_t* tv)
{
  tv->x = t->xx*v->x + t->xy*v->y + t->xz*v->z;
  tv->y = t->xy*v->x + t->yy*v->y + t->yz*v->z;
  tv->z = t->xz*v->x + t->yz*v->y + t->zz*v->z;
}

/// Computes the double dot product x dot t dot y.
/// \memberof symtensor2
static inline real_t symtensor2_ddot(symtensor2_t* t, vector_t* x, vector_t* y)
{
  vector_t ty;
  symtensor2_dot_vector(t, y, &ty);
  return vector_dot(x, &ty);
}

/// Computes the inverse of the symmetric tensor.
/// \memberof symtensor2
static inline void symtensor2_invert(symtensor2_t* t, symtensor2_t* t_inverse)
{
#ifndef NDEBUG
  real_t det_t = symtensor2_det(t);
  ASSERT(!reals_equal(det_t, 0.0));
#endif
  t_inverse->xx = t->yy*t->zz - t->yz*t->yz;
  t_inverse->xy = t->xz*t->yz - t->xy*t->zz;
  t_inverse->xz = t->xy*t->yz - t->xz*t->yy;
  t_inverse->yy = t->xx*t->zz - t->xz*t->xz;
  t_inverse->yz = t->xz*t->xy - t->xx*t->yz;
  t_inverse->zz = t->xx*t->yy - t->xy*t->xy;
}

/// Computes the 3 eigenvalues of the symmetric tensor, storing them in
/// the given array in ascending order.
/// \memberof symtensor2
void symtensor2_get_eigenvalues(symtensor2_t* t, real_t eigenvalues[3]);

/// Computes the 3 eigenvalues and eigenvectors of the symmetric tensor,
/// storing the former as scalars in the given array and the later as
/// vectors in the array eigenvectors.
/// \memberof symtensor2
void symtensor2_get_eigenvectors(symtensor2_t* t,
                                 real_t eigenvalues[3],
                                 vector_t eigenvectors[3]);

/// Writes a text representation of the symmetric tensor to the given stream.
/// \memberof symtensor2
void symtensor2_fprintf(symtensor2_t* t, FILE* stream);

///@}

// Arrays of tensors.
DEFINE_ARRAY(tensor2_array, tensor2_t)
DEFINE_ARRAY(symtensor2_array, symtensor2_t)

#endif

