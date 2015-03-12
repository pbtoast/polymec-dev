// Copyright (c) 2012-2015, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>

#include "cmockery.h"
#include "core/polymec.h"
#include "core/declare_nd_array.h"
#include "core/norms.h"
#include "core/silo_file.h"
#include "core/linear_algebra.h"
#include "geometry/create_uniform_mesh.h"
#include "integrators/krylov_solver.h"

typedef struct 
{
  real_t phi1, phi2, phi3, phi4, phi5, phi6;
  int nx, ny, nz; 
  real_t h; // Grid spacing.
} laplace_t;

static adj_graph_t* laplace_graph(laplace_t* laplace)
{
  int nx = laplace->nx;
  int ny = laplace->ny;
  int nz = laplace->nz;
  int N = nx*ny*nz;

  // Let's be clever. I[i][j][k] -> index
  int* indices = polymec_malloc(sizeof(int) * N);
  for (int i = 0; i < N; ++i)
    indices[i] = i;
  DECLARE_3D_ARRAY(int, I, (void*)indices, nx, ny, nz);
  
  adj_graph_t* g = adj_graph_new(MPI_COMM_WORLD, N);

  // Interior vertices have 6 edges.
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      for (int k = 1; k < nz-1; ++k)
      {
        int v = I[i][j][k];
        adj_graph_set_num_edges(g, v, 6);
      }
    }
  }

  // Boundary vertices have only 1 edge.
  for (int j = 1; j < ny-1; ++j)
  {
    for (int k = 1; k < nz-1; ++k)
    {
      int v1 = I[0][j][k];
      adj_graph_set_num_edges(g, v1, 1);
      int v2 = I[nx-1][j][k];
      adj_graph_set_num_edges(g, v2, 1);
    }
  }
  for (int k = 1; k < nz-1; ++k)
  {
    for (int i = 1; i < nx-1; ++i)
    {
      int v1 = I[i][0][k];
      adj_graph_set_num_edges(g, v1, 1);
      int v2 = I[i][ny-1][k];
      adj_graph_set_num_edges(g, v2, 1);
    }
  }
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      int v1 = I[i][j][0];
      adj_graph_set_num_edges(g, v1, 1);
      int v2 = I[i][j][nz-1];
      adj_graph_set_num_edges(g, v2, 1);
    }
  }

  // Now set'em edges.
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      for (int k = 1; k < nz-1; ++k)
      {
        int v = I[i][j][k];
        int* edges = adj_graph_edges(g, v);
        edges[0] = I[i-1][j][k];
        edges[1] = I[i+1][j][k];
        edges[2] = I[i][j-1][k];
        edges[3] = I[i][j+1][k];
        edges[4] = I[i][j][k-1];
        edges[5] = I[i][j][k+1];
      }
    }
  }
  for (int j = 1; j < ny-1; ++j)
  {
    for (int k = 1; k < nz-1; ++k)
    {
      int v1 = I[0][j][k];
      int* v1_edges = adj_graph_edges(g, v1);
      v1_edges[0] = I[1][j][k];
      int v2 = I[nx-1][j][k];
      int* v2_edges = adj_graph_edges(g, v2);
      v2_edges[0] = I[nx-2][j][k];
    }
  }
  for (int k = 1; k < nz-1; ++k)
  {
    for (int i = 1; i < nx-1; ++i)
    {
      int v1 = I[i][0][k];
      int* v1_edges = adj_graph_edges(g, v1);
      v1_edges[0] = I[i][1][k];
      int v2 = I[i][ny-1][k];
      int* v2_edges = adj_graph_edges(g, v2);
      v2_edges[0] = I[i][ny-2][k];
    }
  }
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      int v1 = I[i][j][0];
      int* v1_edges = adj_graph_edges(g, v1);
      v1_edges[0] = I[i][j][1];
      int v2 = I[i][j][nz-1];
      int* v2_edges = adj_graph_edges(g, v2);
      v2_edges[0] = I[i][j][nz-2];
    }
  }
  polymec_free(I);
  return g;
}

static int laplace_Ay(void* context, real_t t, real_t* y, real_t* Ay)
{
  // A is the matrix representing the Laplacian operator on our uniform mesh, which 
  // in 3D is just the finite difference stencil.
  laplace_t* laplace = context;
  int nx = laplace->nx;
  int ny = laplace->ny;
  int nz = laplace->nz;
  int N = nx*ny*nz;

  // Zero everything and make 3D arrays for easy referencing.
  memset(Ay, 0, sizeof(real_t) * N);
  DECLARE_3D_ARRAY(real_t, yijk, (void*)y, nx, ny, nz);
  DECLARE_3D_ARRAY(real_t, Ayijk, (void*)Ay, nx, ny, nz);

  real_t h2 = laplace->h * laplace->h;

  // Compute the interior part of Ay.
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      for (int k = 1; k < nz-1; ++k)
      {
        Ayijk[i][j][k] -= 6.0 * yijk[i][j][k]/h2; // Diagonal entry.
        Ayijk[i][j][k] += yijk[i-1][j][k]/h2;
        Ayijk[i][j][k] += yijk[i+1][j][k]/h2;
        Ayijk[i][j][k] += yijk[i][j-1][k]/h2;
        Ayijk[i][j][k] += yijk[i][j+1][k]/h2;
        Ayijk[i][j][k] += yijk[i][j][k-1]/h2;
        Ayijk[i][j][k] += yijk[i][j][k+1]/h2;
      }
    }
  }

  // Now handle Dirichlet boundary values.
  for (int j = 1; j < ny-1; ++j)
  {
    for (int k = 1; k < nz-1; ++k)
    {
      Ayijk[0][j][k] += 0.5*yijk[0][j][k];
      Ayijk[1][j][k] += 0.5*yijk[0][j][k];
      Ayijk[nx-1][j][k] += 0.5*yijk[nx-1][j][k];
      Ayijk[nx-2][j][k] += 0.5*yijk[nx-2][j][k];
    }
  }
  for (int k = 1; k < nz-1; ++k)
  {
    for (int i = 1; i < nx-1; ++i)
    {
      Ayijk[i][0][k] += 0.5*yijk[i][0][k];
      Ayijk[i][1][k] += 0.5*yijk[i][1][k];
      Ayijk[i][ny-1][k] += 0.5*yijk[i][ny-1][k];
      Ayijk[i][ny-2][k] += 0.5*yijk[i][ny-2][k];
    }
  }
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      Ayijk[i][j][0] += 0.5*yijk[i][j][0];
      Ayijk[i][j][1] += 0.5*yijk[i][j][1];
      Ayijk[i][j][nz-1] += 0.5*yijk[i][j][nz-1];
      Ayijk[i][j][nz-2] += 0.5*yijk[i][j][nz-2];
    }
  }

//printf("y = ");
//vector_fprintf(y, N, stdout);
//printf("\n");
//printf("Ay = ");
//vector_fprintf(Ay, N, stdout);
//printf("\n");
  return 0;
}

static krylov_solver_t* laplace_solver_new()
{
  MPI_Comm comm = MPI_COMM_WORLD;
  real_t L = 1.0;
  laplace_t* laplace = polymec_malloc(sizeof(laplace_t));
  laplace->nx = 4; 
  laplace->ny = 4; 
  laplace->nz = 4;
  laplace->phi1 = 0.0;
  laplace->phi2 = 0.0;
  laplace->phi3 = 0.0;
  laplace->phi4 = 0.0;
  laplace->phi5 = 0.0;
  laplace->phi6 = 1.0;
  laplace->h = L / laplace->nx;

  int N = laplace->nx * laplace->ny * laplace->nz;
  return krylov_solver_new(comm, N, 0, laplace, laplace_Ay, polymec_free, 
                           KRYLOV_GMRES, 15, 3);
}

static real_t* laplace_rhs(laplace_t* laplace)
{
  int nx = laplace->nx;
  int ny = laplace->ny;
  int nz = laplace->nz;
  int N = nx*ny*nz;

  real_t* rhs = polymec_malloc(sizeof(real_t) * N);
  memset(rhs, 0, sizeof(real_t) * N);

  DECLARE_3D_ARRAY(real_t, bijk, (void*)rhs, laplace->nx, laplace->ny, laplace->nz);
  for (int j = 1; j < ny-1; ++j)
  {
    for (int k = 1; k < nz-1; ++k)
    {
      bijk[0][j][k] = laplace->phi1;
      bijk[nx-1][j][k] = laplace->phi2;
    }
  }
  for (int k = 1; k < nz-1; ++k)
  {
    for (int i = 1; i < nx-1; ++i)
    {
      bijk[i][0][k] = laplace->phi3;
      bijk[i][ny-1][k] = laplace->phi4;
    }
  }
  for (int i = 1; i < nx-1; ++i)
  {
    for (int j = 1; j < ny-1; ++j)
    {
      bijk[i][j][0] = laplace->phi5;
      bijk[i][j][nz-1] = laplace->phi6;
    }
  }

//printf("b = ");
//vector_fprintf(rhs, N, stdout);
//printf("\n");
  return rhs;
}

void test_no_precond_laplace_ctor(void** state)
{
  krylov_solver_t* krylov = laplace_solver_new();
  krylov_solver_free(krylov);
}

void test_block_jacobi_precond_laplace_ctor(void** state)
{
  krylov_solver_t* krylov = laplace_solver_new();
  laplace_t* laplace = krylov_solver_context(krylov);
  adj_graph_t* g = laplace_graph(laplace);
  krylov_solver_set_preconditioner(krylov, KRYLOV_BLOCK_JACOBI, g);
  krylov_solver_free(krylov);
  adj_graph_free(g);
}

void test_lu_precond_laplace_ctor(void** state)
{
  krylov_solver_t* krylov = laplace_solver_new();
  laplace_t* laplace = krylov_solver_context(krylov);
  adj_graph_t* g = laplace_graph(laplace);
  krylov_solver_set_preconditioner(krylov, KRYLOV_LU, g);
  krylov_solver_free(krylov);
  adj_graph_free(g);
}

void test_laplace_solve(void** state, krylov_solver_t* krylov)
{
  // Set up the problem.
  krylov_solver_set_tolerances(krylov, 1e-7, 1e-13);
  laplace_t* laplace = krylov_solver_context(krylov);
  real_t* x = laplace_rhs(laplace);
  int N = laplace->nx * laplace->ny * laplace->nz;

  // Solve it.
  int num_iters;
  bool solved = krylov_solver_solve(krylov, 0.0, x, &num_iters);
  if (!solved)
  {
    krylov_solver_diagnostics_t diagnostics;
    krylov_solver_get_diagnostics(krylov, &diagnostics);
    krylov_solver_diagnostics_fprintf(&diagnostics, stdout);
  }
  assert_true(solved);
  log_info("num iterations = %d\n", num_iters);
  assert_true(num_iters < 10);

  // Evaluate the 2-norm of the residual.
  real_t* b = laplace_rhs(laplace);
  real_t R[N];
  krylov_solver_eval_residual(krylov, 0.0, x, b, R);
  real_t L2 = l2_norm(R, N);
  log_info("||R||_L2 = %g\n", L2);
  assert_true(L2 < sqrt(1e-7));

  krylov_solver_free(krylov);
  polymec_free(x);
  polymec_free(b);
}

void test_no_precond_laplace_solve(void** state)
{
  krylov_solver_t* krylov = laplace_solver_new();
  test_laplace_solve(state, krylov);
}

void test_block_jacobi_precond_laplace_solve(void** state)
{
  krylov_solver_t* krylov = laplace_solver_new();
  laplace_t* laplace = krylov_solver_context(krylov);
  adj_graph_t* g = laplace_graph(laplace);
  krylov_solver_set_preconditioner(krylov, KRYLOV_BLOCK_JACOBI, g);
  adj_graph_free(g);
  test_laplace_solve(state, krylov);
}

void test_lu_precond_laplace_solve(void** state)
{
  // Set up the problem.
  krylov_solver_t* krylov = laplace_solver_new();
  laplace_t* laplace = krylov_solver_context(krylov);
  adj_graph_t* g = laplace_graph(laplace);
  krylov_solver_set_preconditioner(krylov, KRYLOV_LU, g);
  adj_graph_free(g);
  test_laplace_solve(state, krylov);
}

int main(int argc, char* argv[]) 
{
  polymec_init(argc, argv);
  const UnitTest tests[] = 
  {
    unit_test(test_no_precond_laplace_ctor),
    unit_test(test_block_jacobi_precond_laplace_ctor),
    unit_test(test_lu_precond_laplace_ctor),
    unit_test(test_no_precond_laplace_solve),
    unit_test(test_block_jacobi_precond_laplace_solve),
    unit_test(test_lu_precond_laplace_solve)
  };
  return run_tests(tests);
}

