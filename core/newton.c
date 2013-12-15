// Copyright (c) 2012-2013, Jeffrey N. Johnson
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

#include "core/newton.h"
#include "core/linear_algebra.h"
#include "core/norms.h"
#include "kinsol/kinsol.h"
#include "kinsol/kinsol_dense.h"

struct newton_solver_t 
{
  // Nonlinear system information.
  int dim;
  void* context;
  void (*dtor)(void*);

  // Nonlinear solver.
  void* kinsol;

  // Work vectors.
  N_Vector x, x_scale, F_scale;
};

newton_solver_t* newton_solver_new(int dimension,
                                   void* context,
                                   newton_system_func system_func,
                                   void (*context_dtor)(void*))
{
  return newton_solver_new_with_jacobian(dimension, context, system_func,
                                         NULL, context_dtor);
}

newton_solver_t* newton_solver_new_with_jacobian(int dimension,
                                                 void* context,
                                                 newton_system_func system_func,
                                                 newton_jacobian_func jacobian_func,
                                                 void (*context_dtor)(void*))
{
  ASSERT(dimension > 0);
  ASSERT(system_func != NULL);
  newton_solver_t* solver = malloc(sizeof(newton_solver_t));
  solver->dim = dimension;
  solver->context = context;
  solver->dtor = context_dtor;
  solver->x = N_VNew_Serial(dimension);
  solver->x_scale = N_VNew_Serial(dimension);
  solver->F_scale = N_VNew_Serial(dimension);
  solver->kinsol = KINCreate();
  KINInit(solver->kinsol, system_func, solver->x);
  KINDense(solver->kinsol, dimension);

  // Do we have a Jacobian function?
  if (jacobian_func != NULL)
    KINDlsSetDenseJacFn(solver->kinsol, jacobian_func);

  // Use exact Newton?
  // KINSetMaxSetupCalls(solver->kinsol, 1);

  return solver;
}

void newton_solver_free(newton_solver_t* solver)
{
  N_VDestroy(solver->x);
  N_VDestroy(solver->x_scale);
  N_VDestroy(solver->F_scale);
  KINFree(&(solver->kinsol));
  if ((solver->context != NULL) && (solver->dtor != NULL))
    solver->dtor(solver->context);
  free(solver);
}

int newton_solver_dimension(newton_solver_t* solver)
{
  return solver->dim;
}

void newton_solver_set_tolerances(newton_solver_t* solver, double norm_tolerance, double step_tolerance)
{
  ASSERT(norm_tolerance > 0.0);
  ASSERT(step_tolerance > 0.0);
  KINSetFuncNormTol(solver->kinsol, norm_tolerance);
  KINSetScaledStepTol(solver->kinsol, step_tolerance);
}

void newton_solver_set_max_iterations(newton_solver_t* solver, int max_iterations)
{
  ASSERT(max_iterations > 0);
  KINSetNumMaxIters(solver->kinsol, max_iterations);
}

bool newton_solver_solve(newton_solver_t* solver, double* X, int* num_iterations)
{
  // Call the scaled solve.
  return newton_solver_solve_scaled(solver, X, NULL, NULL, num_iterations);
}

bool newton_solver_solve_scaled(newton_solver_t* solver, double* X, double* x_scale, double* F_scale, int* num_iterations)
{
  // Suspend the currently active floating point exceptions for now.
  polymec_suspend_fpe_exceptions();

  // Set the x_scale and F_scale vectors.
  if (x_scale != NULL)
    memcpy(NV_DATA_S(solver->x_scale), x_scale, sizeof(double) * solver->dim);
  else
  {
    for (int i = 0; i < solver->dim; ++i)
      NV_Ith_S(solver->x_scale, i) = 1.0;
  }
  if (F_scale != NULL)
    memcpy(NV_DATA_S(solver->F_scale), F_scale, sizeof(double) * solver->dim);
  else
  {
    for (int i = 0; i < solver->dim; ++i)
      NV_Ith_S(solver->F_scale, i) = 1.0;
  }

  // Copy the values in X to the internal solution vector.
  memcpy(NV_DATA_S(solver->x), X, sizeof(double) * solver->dim);

  // Solve.
  int status = KINSol(solver->kinsol, solver->x, KIN_LINESEARCH, 
                      solver->x_scale, solver->F_scale);

  // Reinstate the floating point exceptions.
  polymec_restore_fpe_exceptions();

  if ((status == KIN_SUCCESS) || (status == KIN_INITIAL_GUESS_OK))
  {
    // Get the number of iterations it took.
    long num_iters;
    KINGetNumNonlinSolvIters(solver->kinsol, &num_iters);
    *num_iterations = (int)num_iters;

    // Copy the data back into X.
    memcpy(X, NV_DATA_S(solver->x), sizeof(double) * solver->dim);
    return true;
  }

  // Failed!
  return false;
}

static bool in_range(double x, double a, double b)
{
  return ((MIN(a, b) <= x) && (x <= MAX(a, b)));
}

// This version of Brent's method was taken from Wikipedia.
double brent_solve(double (*F)(void*, double), void* context, double x1, double x2, double tolerance, int max_iters)
{
  static const double delta = 1e-8;
  static const double eps = 1e-8;
  double a = x1, b = x2;
  double fa = F(context, a);
  double fb = F(context, b);
  if (fa * fb >= 0.0)
    polymec_error("brent_solve: Root is not bracketed by [x1, x2].");
  if (fabs(fa) < fabs(fb))
  {
    double tmp1 = b, tmp2 = fb;
    b = a;
    fb = fa;
    a = tmp1;
    fa = tmp2;
  }
  double c = a;
  bool mflag = true;
  double fs, d, s;
  int num_iter = 0;
  do
  {
    double fc = F(context, c);
    if ((fa != fc) && (fb != fc))
    {
      s = a*fb*fc / ((fa-fb)*(fa-fc)) + 
          b*fa*fc / ((fb-fa)*(fb-fc)) + 
          c*fa*fb / ((fc-fa)*(fc-fb));
    }
    else
    {
      s = b - fb * (b-a)/(fb-fa);
    }
    if (!in_range(s, (3.0*a + b)/4.0, b) || 
        (mflag && fabs(s-b) >= 0.5*fabs(b-c)) ||
        (!mflag && fabs(s-b) >= 0.5*fabs(c-d)) || 
        (mflag && fabs(b-c) < delta) ||
        (!mflag && fabs(c-d) < delta))
    {
      s = 0.5 * (a + b);
      mflag = true;
    }
    else
      mflag = false;
    fs = F(context, s);
    d = c; // First assignment of d (protected by mflag).
    c = b;
    if (fa * fs < 0.0) 
    {
      b = s;
      fb = fs;
    }
    else
    {
      a = s;
      fa = fs;
    }
    if (fabs(fa) < fabs(fb))
    {
      double tmp1 = b, tmp2 = fb;
      b = a;
      fb = fa;
      a = tmp1;
      fa = tmp2;
    }
    ++num_iter;
  }
  while ((MAX(fabs(fb), fabs(fs)) > tolerance) && (num_iter < max_iters));
  return (fabs(fb) < fabs(fs)) ? b : s;
}

