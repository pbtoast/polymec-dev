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

#include "sundials/sundials_spgmr.h"
#include "sundials/sundials_spbcgs.h"
#include "sundials/sundials_sptfqmr.h"

#include "core/krylov_solver.h"
#include "core/sundials_helpers.h"

typedef enum
{
  GMRES, BICGSTAB, TFQMR
} solver_type;

struct krylov_solver_t 
{
  solver_type type;
  MPI_Comm comm;
  char* name;
  void* context;
  krylov_solver_vtable vtable;
  real_t delta;
  int N, max_krylov_dim, max_restarts;

  SpgmrMem gmres;
  SpbcgMem bicgstab;
  SptfqmrMem tfqmr;

  N_Vector X, B;
};

krylov_solver_t* gmres_krylov_solver_new(MPI_Comm comm, 
                                         void* context,
                                         krylov_solver_vtable vtable,
                                         int N,
                                         int max_krylov_dim,
                                         int max_restarts)
{
  ASSERT(vtable.ax != NULL);
  ASSERT(N > 0);
  ASSERT(max_krylov_dim >= 3);
  ASSERT(max_restarts >= 0);

  krylov_solver_t* solver = polymec_malloc(sizeof(krylov_solver_t));
  solver->name = string_dup("GMRES");
  solver->type = GMRES;
  solver->comm = comm;
  solver->context = context;
  solver->vtable = vtable;
  solver->N = N;
  solver->max_krylov_dim = max_krylov_dim;
  solver->max_restarts = max_restarts;
  solver->delta = 1e-8;

  solver->X = N_VNew(comm, N);
  solver->B = N_VNew(comm, N);
  solver->gmres = SpgmrMalloc(max_krylov_dim, solver->X);

  return solver;
}
  
krylov_solver_t* bicgstab_krylov_solver_new(MPI_Comm comm,
                                            void* context,
                                            krylov_solver_vtable vtable,
                                            int N,
                                            int max_krylov_dim)
{
  ASSERT(vtable.ax != NULL);
  ASSERT(N > 0);
  ASSERT(max_krylov_dim >= 3);

  krylov_solver_t* solver = polymec_malloc(sizeof(krylov_solver_t));
  solver->name = string_dup("BiCGSTAB");
  solver->type = BICGSTAB;
  solver->comm = comm;
  solver->context = context;
  solver->vtable = vtable;
  solver->N = N;
  solver->max_krylov_dim = max_krylov_dim;
  solver->delta = 1e-8;

  solver->X = N_VNew(comm, N);
  solver->B = N_VNew(comm, N);
  solver->bicgstab = SpbcgMalloc(max_krylov_dim, solver->X);

  return solver;
}
  
krylov_solver_t* tfqmr_krylov_solver_new(MPI_Comm comm,
                                         void* context,
                                         krylov_solver_vtable vtable,
                                         int N,
                                         int max_krylov_dim)
{
  ASSERT(vtable.ax != NULL);
  ASSERT(N > 0);
  ASSERT(max_krylov_dim >= 3);

  krylov_solver_t* solver = polymec_malloc(sizeof(krylov_solver_t));
  solver->name = string_dup("TFQMR");
  solver->type = TFQMR;
  solver->comm = comm;
  solver->context = context;
  solver->vtable = vtable;
  solver->N = N;
  solver->max_krylov_dim = max_krylov_dim;
  solver->delta = 1e-8;

  solver->X = N_VNew(comm, N);
  solver->B = N_VNew(comm, N);
  solver->tfqmr = SptfqmrMalloc(max_krylov_dim, solver->X);

  return solver;
}
 
void krylov_solver_free(krylov_solver_t* solver)
{
  if (solver->type == GMRES)
  {
    SpgmrFree(solver->gmres);
  }
  else if (solver->type == BICGSTAB)
  {
    SpbcgFree(solver->bicgstab);
  }
  else
  {
    SptfqmrFree(solver->tfqmr);
  }
  polymec_free(solver->name);
  if ((solver->context != NULL) && (solver->vtable.dtor != NULL))
    solver->vtable.dtor(solver->context);
  polymec_free(solver);
}

void krylov_solver_set_tolerance(krylov_solver_t* solver, real_t delta)
{
  ASSERT(delta > 0.0);
  solver->delta = delta;
}

// This implements the A*X function in terms understandable to Sundials.
static int krylov_ax(void* solver_ptr, N_Vector x, N_Vector Ax)
{
  krylov_solver_t* solver = solver_ptr;
  return solver->vtable.ax(solver->context, NV_DATA(x), NV_DATA(Ax));
}

bool krylov_solver_solve(krylov_solver_t* solver, real_t* B, real_t* X, 
                         real_t* res_norm, int* num_iters, int* num_precond)
{
  if (solver->type == GMRES)
  {
    int stat = SpgmrSolve(solver->gmres, solver, solver->X, 
                          solver->B, PREC_NONE, MODIFIED_GS, solver->delta, 
                          solver->max_restarts, NULL, NULL, NULL, krylov_ax, NULL, res_norm,
                          num_iters, num_precond);
    return ((stat == SPGMR_SUCCESS) || (stat == SPGMR_RES_REDUCED));
  }
  else if (solver->type == BICGSTAB)
  {
    int stat = SpbcgSolve(solver->bicgstab, solver, solver->X, 
                          solver->B, PREC_NONE, solver->delta, NULL, 
                          NULL, NULL, krylov_ax, NULL, res_norm,
                          num_iters, num_precond);
    return ((stat == SPBCG_SUCCESS) || (stat == SPBCG_RES_REDUCED));
  }
  else
  {
    int stat = SptfqmrSolve(solver->tfqmr, solver, solver->X, 
                            solver->B, PREC_NONE, solver->delta, NULL, 
                            NULL, NULL, krylov_ax, NULL, res_norm,
                            num_iters, num_precond);
    return ((stat == SPTFQMR_SUCCESS) || (stat == SPTFQMR_RES_REDUCED));
  }
}

