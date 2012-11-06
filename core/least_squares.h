#ifndef ARBI_LEAST_SQUARES_H
#define ARBI_LEAST_SQUARES_H

#include "core/arbi.h"
#include "core/point.h"

#ifdef __cplusplus
extern "C" {
#endif

// This multi-index represents a pth-order multinomial term of the given 
// orders in x, y, and z.
typedef struct multi_index_t multi_index_t;

// Creates a multi-index object with the given order.
// Objects of this type are garbage-collected.
multi_index_t* multi_index_new(int p);

// Returns the order of the polynomial basis of the multi-index.
int multi_index_order(multi_index_t* m);

// Returns the number of terms in the polynomial basis of the multi-index.
int multi_index_size(multi_index_t* m);

// Increments the given multi-index to move to the next term.
// Returns true if the multi-index can be incremented again, false if not.
bool multi_index_next(multi_index_t* m, int* x_order, int* y_order, int* z_order);

// Resets the given multi-index to its first term.
void multi_index_reset(multi_index_t* m);

// This is a weighting function used for least-squares systems. It returns 
// a value given a Euclidean distance and a spacing parameter.
typedef double (*ls_weighting_func_t)(double);

// Returns the size of the least-squares polynomial basis of order p.
int poly_ls_basis_size(int p);

// Allocates sufficient storage for a moment matrix (or its transpose) for 
// a least squares fit of polynomial order p. This storage must be freed with 
// a call to free(). 
// NOTE: This function does not compute the coefficients of the moment matrix. 
double* allocate_poly_ls_moment_matrix(int p);

// Allocates sufficient storage for the right-hand-side vector for a least
// squares fit of polynomial order p. This storage must be freed with a call to free(). 
// NOTE: This function does not compute the coefficients of the vector. 
double* allocate_poly_ls_rhs_vector(int p);

// Computes the pth-order polynomial basis vector for the given point.
void compute_poly_ls_basis_vector(int p, point_t* point, double* basis);

// Computes the least squares system for a pth-order polynomial fit to a set 
// of scattered point data, centered about the point x0. This computes the 
// moment matrix and the right-hand side vector for a polynomial fit to the 
// given (scalar) data on the given points.
void compute_poly_ls_system(int p, point_t* x0, point_t* points, int num_points, 
                            double* data, double* moment_matrix, double* rhs);

// This computes the weighted least squares system for the weighting function W.
void compute_weighted_poly_ls_system(int p, ls_weighting_func_t W, point_t* x0, point_t* points, int num_points, 
                                     double* data, double* moment_matrix, double* rhs);

// This class represents a shape function basis for a polynomial least-squares 
// fit. A shape function maps a set of data (associated a given set of points in 
// space) to a value interpolated at a given point.
// Objects of this type are garbage-collected.
typedef struct poly_ls_shape_basis_t poly_ls_shape_basis_t;

// Create a new shape function for a polynomial least-squares fit of order p, 
// with an optional weighting function.
poly_ls_shape_basis_t* poly_ls_shape_basis_new(int p);

// Compute the values of the shape function basis at the point x0, 
// given a set of points.
void poly_ls_shape_basis_compute(poly_ls_shape_basis_t* N, point_t* x0, point_t* points, int num_points, double* values);

#ifdef __cplusplus
}
#endif

#endif
