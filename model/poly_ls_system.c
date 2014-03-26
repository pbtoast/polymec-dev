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

#include "core/polymec.h"
#include "core/array.h"
#include "core/polynomial.h"
#include "core/linear_algebra.h"
#include "model/poly_ls_system.h"

struct poly_ls_system_t 
{
  polynomial_t* poly;
  ptr_array_t* equations;
  ptr_array_t* points;
};

poly_ls_system_t* poly_ls_system_new(int p, point_t* x0)
{
  ASSERT(p >= 0);
  poly_ls_system_t* sys = malloc(sizeof(poly_ls_system_t));
  int dim = polynomial_basis_dim(p);
  real_t coeffs[dim];
  for (int i = 0; i < dim; ++i)
    coeffs[i] = 1.0;
  sys->poly = polynomial_new(p, coeffs, x0);
  sys->equations = ptr_array_new();
  sys->points = ptr_array_new();
  return sys;
}

void poly_ls_system_free(poly_ls_system_t* sys)
{
  sys->poly = NULL;
  ptr_array_free(sys->equations);
  ptr_array_free(sys->points);
  free(sys);
}

// This constructs an array of coefficients representing an equation, 
// returning the allocated memory. There are dim+1 coefficients: dim for the 
// polynomial basis, and 1 for the RHS.
static real_t* append_equation(poly_ls_system_t* sys, point_t* x)
{
  int dim = polynomial_basis_dim(polynomial_degree(sys->poly));
  real_t* eq = malloc(sizeof(real_t) * (dim + 1));
  ptr_array_append_with_dtor(sys->equations, eq, DTOR(free));
  ptr_array_append(sys->points, x);
  return eq;
}

void poly_ls_system_add_interpolated_datum(poly_ls_system_t* sys, real_t u, point_t* x)
{
  real_t* eq = append_equation(sys, x);
  point_t* x0 = polynomial_x0(sys->poly);

  // Left hand side -- powers of (x - x0) in the polynomial.
  real_t coeff;
  int pos = 0, x_pow, y_pow, z_pow, i = 0;
  while (polynomial_next(sys->poly, &pos, &coeff, &x_pow, &y_pow, &z_pow))
  {
    real_t X = x->x - x0->x;
    real_t Y = x->y - x0->y;
    real_t Z = x->z - x0->z;
    eq[i++] = pow(X, x_pow) * pow(Y, y_pow) * pow(Z, z_pow);
  }

  // Right hand side -- u.
  eq[i] = u;
}

void poly_ls_system_add_robin_bc(poly_ls_system_t* sys, real_t alpha, real_t beta, vector_t* n, real_t gamma, point_t* x)
{
  real_t* eq = append_equation(sys, x);
  point_t* x0 = polynomial_x0(sys->poly);

  // Left hand side -- powers of (x - x0) in the polynomial expression.
  real_t coeff;
  int pos = 0, x_pow, y_pow, z_pow, i = 0;
  while (polynomial_next(sys->poly, &pos, &coeff, &x_pow, &y_pow, &z_pow))
  {
    real_t X = x->x - x0->x;
    real_t Y = x->y - x0->y;
    real_t Z = x->z - x0->z;
    real_t u_term = alpha * pow(X, x_pow) * pow(Y, y_pow) * pow(Z, z_pow);
    real_t dudx_term = x_pow * pow(X, x_pow-1) * pow(Y, y_pow) * pow(Z, z_pow);
    real_t dudy_term = y_pow * pow(X, x_pow) * pow(Y, y_pow-1) * pow(Z, z_pow);
    real_t dudz_term = z_pow * pow(X, x_pow) * pow(Y, y_pow) * pow(Z, z_pow-1);
    real_t n_o_grad_u_term = n->x * dudx_term + n->y * dudy_term + n->z * dudz_term;
    eq[i++] = alpha * u_term + beta * n_o_grad_u_term;
  }

  // Right hand side -- gamma.
  eq[i] = gamma;
}

void poly_ls_system_clear(poly_ls_system_t* sys)
{
  ptr_array_clear(sys->equations);
}

int poly_ls_system_num_equations(poly_ls_system_t* sys)
{
  return sys->equations->size;
}

static void solve_direct_least_squares(int p, ptr_array_t* equations, real_t* x)
{
  int N = equations->size;
  int dim = polynomial_basis_dim(p);

  real_t A[N*dim], b[N];
  for (int i = 0; i < N; ++i)
  {
    real_t* eq = equations->data[i];
    for (int j = 0; j < dim; ++j)
      A[N*j+i] = eq[j];
    b[i] = eq[dim];
  }
  int one = 1, jpivot[N], rank, info;
  int lwork = MAX(N*dim+3*N+1, 2*N*dim+1); // unblocked strategy
  real_t rcond = 0.01, work[lwork];
  rgelsy(&N, &dim, &one, A, &N, b, &N, jpivot, &rcond, &rank, work, &lwork, &info);
  ASSERT(info != 0);

  // Copy the answer from b to x.
  memcpy(x, b, sizeof(real_t) * dim);
}

// The following functions contain logic that implements the Coupled Least 
// Squares algorithm for the construction of compact-stencil high-order 
// polynomial fits, as discussed by Haider (2011).
//
// The following concepts are used in this algorithm:
// - A neighborhood of cells W(i) is a set of cells on which a least squares 
//   fit is performed for a quantity u in the vicinity of a cell i. The 
//   neighborhood V(i) refers to the set of cells adjacent to and including i.
// - The moments z(k), associated with a neighborhood W, are volume integrals 
//   involving the kth-order tensor products of differences between the 
//   integration coordinates and the cell center coordinates. Within a given 
//   cell, z(k) is a symmetric tensor of degree k.
// - The linear map w(m|k), given a vector of N cell-averaged values of u on 
//   a neighborhood W, produces the components of the mth derivative of the 
//   degree-k polynomial representation of u within that neighborhood, for 
//   m <= k. These derivatives form a symmetric tensor of rank m. In 3D, such 
//   a tensor has (3**m - 3)/2 coefficients for m > 1, 3 coefficients for m = 1, 
//   and 1 coefficient for m = 0. Thus, the map w(m|k) can be represented by a 
//   (3**m - 3)/2 x N matrix (for m > 1, anyway). The product of this matrix 
//   with the N cell-averaged values of u in the neighborhood W(i) about a 
//   cell i is a vector containing the (3**m ­ 3)/2 spatial derivatives of 
//   u's polynomial representation in i.

// Given a set of N cell averages in the neighborhood W(i), performs a least 
// squares fit to reconstruct the value of the polynomial fit at the center of 
// celli, storing it in w00.
static void reconstruct_cls_value(real_t* cell_averages, int N, real_t* w00)
{
}

// This function constructs the J(k+1) "antiderivative" operator 
// in Haider (2011). This is an N x (3**(k+1) - 3)/2 matrix.
static void construct_Jk1(int k, int N, real_t* wkk, real_t* zk1_moments, real_t* J)
{
}

// Given the linear map w(k|k) for the neighborhood W(i) and the moments 
// z(k+1) for each cell in that neighborhood, this function calculates the 
// components of the linear map w(k+1|k+1). Arguments:
// k - the degree of the derivatives used to construct the k+1 derivatives.
// N - the number of cells in the neighborhood W(i).
// wkk - the components of the linear map w(k|k) in column-major order.
//       There are (3**k - 3) * N / 2 components in this array. In particular, 
//       w(0|0) contains N values that, when dotted with the cell averages in 
//       W(i), yield the value of the polynomial at the cell center i.
// zk1_moments - An array containing the N moment tensors z(k+1) for the cells 
//               in the neighborhood W(i), stored in cell-major order. Each 
//               tensor in the array is stored in column-major order.
//               There are N * (3**(k+1)-3)/2 components in this array.
// wk1k1 - an array that will store the components of the linear map w(k+1|k+1)
//         in column-major order.
void reconstruct_cls_derivatives(int k, int N, real_t* wkk, 
                                 real_t* zk1_moments, real_t* wk1k1)
{
}

static void solve_coupled_least_squares(int p, ptr_array_t* equations, real_t* x)
{
  // Compute a 1-exact derivative directly on our stencil.

  // For m = 1 to m = p - 1:
}

void poly_ls_system_solve(poly_ls_system_t* sys, real_t* x)
{
  // Solve the least squares system of N equations.
  int p = polynomial_degree(sys->poly);
  int N = sys->equations->size;
  int dim = polynomial_basis_dim(p);

  if (N > dim)
    solve_direct_least_squares(p, sys->equations, x);
  else
    solve_coupled_least_squares(p, sys->equations, x);
}

