// Copyright (c) 2012-2016, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef POLYMEC_DAE_INTEGRATOR_H
#define POLYMEC_DAE_INTEGRATOR_H

#include "core/polymec.h"
#include "core/adj_graph.h"
#include "integrators/newton_pc.h"

// This type indicates whether a given equation in a DAE system is 
// algebraic or differential.
typedef enum
{
  DAE_ALGEBRAIC,
  DAE_DIFFERENTIAL
} dae_equation_t;

// This can be passed as an argument to dae_integrator_new to indicate that 
// all equations are algebraic.
extern dae_equation_t* DAE_ALL_ALGEBRAIC;

// This can be passed as an argument to dae_integrator_new to indicate that 
// all equations are differential.
extern dae_equation_t* DAE_ALL_DIFFERENTIAL;

// This type indicates constraints (if any) on equations in a DAE system.
typedef enum
{
  DAE_UNCONSTRAINED,
  DAE_NEGATIVE,
  DAE_NONPOSITIVE,
  DAE_NONNEGATIVE,
  DAE_POSITIVE
} dae_constraint_t;

// This can be passed as an argument to dae_integrator_new to indicate that 
// no equations have constraints.
extern dae_constraint_t* DAE_ALL_UNCONSTRAINED;

// This can be passed as an argument to dae_integrator_new to indicate that 
// all equations have solution values that are negative.
extern dae_constraint_t* DAE_ALL_NEGATIVE;

// This can be passed as an argument to dae_integrator_new to indicate that 
// all equations have solution values that are non-positive.
extern dae_constraint_t* DAE_ALL_NONPOSITIVE;

// This can be passed as an argument to dae_integrator_new to indicate that 
// all equations have solution values that are non-negative.
extern dae_constraint_t* DAE_ALL_NONNEGATIVE;

// This can be passed as an argument to dae_integrator_new to indicate that 
// all equations have solution values that are positive.
extern dae_constraint_t* DAE_ALL_POSITIVE;

// Types of Krylov solver to use for our DAE method.
typedef enum 
{
  DAE_GMRES,    // Generalized minimum residual Krylov solver
  DAE_BICGSTAB, // Stabilized Biconjugate Gradient Krylov solver
  DAE_TFQMR     // Transpose-Free QMR Krylov solver
} dae_krylov_t;

// This function evaluates the residual of a system of differential-algebraic 
// equations (DAEs).
typedef int (*dae_integrator_residual_func)(void* context, real_t t, real_t* U, real_t* U_dot, real_t* F);

// This function computes the Jacobian J = dF/dU + alpha * dF/d(U_dot),
// where F(t, U, U_dot) is the residual and U is the solution vector 
// (and U_dot = dx/dt).
typedef int (*dae_integrator_Jv_func)(void* context, real_t t, real_t* U, real_t* U_dot, real_t* R, 
                                      real_t* v, real_t* Jv, real_t alpha, real_t* tmp1, real_t* tmp2);

// This function evaluates error weights for use in the WRMS error norm.
typedef void (*dae_integrator_error_weight_func)(void* context, real_t* y, real_t* weights);

// This function destroys the state (context).
typedef void (*dae_integrator_dtor)(void* context);

// This virtual table determines the behavior of the time integrator.
typedef struct
{
  // This function evaluates the residual function for a system of 
  // DAEs at time t with solution U and time derivative U_dot, storing it 
  // in F. It should return 0 on success, 1 for a recoverable error, -1 
  // for a fatal error.
  dae_integrator_residual_func residual;

  // Optional specification of Jacobian * vector.
  dae_integrator_Jv_func Jv;

  // This (optional) function destroys the state (context) when the time integrator 
  // is destroyed.
  dae_integrator_dtor dtor;

} dae_integrator_vtable;

// This class provides an abstract interface for integrating systems of 
// nonlinear differential equations. 
typedef struct dae_integrator_t dae_integrator_t;

// Creates an integrator that uses a Newton-Krylov method to solve a system of 
// Differential Algebraic Equations (DAE) with a given maximum subspace 
// dimension of max_krylov_dim. The equation_types array (of length num_local_values)
// indicates whether each equation in the system is algebraic (DAE_ALGEBRAIC) 
// or differential (DAE_DIFFERENTIAL). The constraints array indicates any 
// constraints on the solution for a given component (DAE_UNCONSTRAINED, 
// DAE_NEGATIVE, DAE_NONPOSITIVE, DAE_NONNEGATIVE, DAE_POSITIVE). See the 
// DAE_ALL* symbols above for shorthand ways of expressing systems with 
// simple equation types and constraints.
dae_integrator_t* dae_integrator_new(int order,
                                     MPI_Comm comm,
                                     dae_equation_t* equation_types,
                                     dae_constraint_t* constraints,
                                     int num_local_values,
                                     int num_remote_values,
                                     void* context,
                                     dae_integrator_vtable vtable,
                                     newton_pc_t* precond,
                                     dae_krylov_t solver_type,
                                     int max_krylov_dim);

// Frees a time integrator.
void dae_integrator_free(dae_integrator_t* integrator);

// Returns the context object for this integrator.
void* dae_integrator_context(dae_integrator_t* integrator);

// Returns the order of the integration method.
int dae_integrator_order(dae_integrator_t* integrator);

// Gets an internal pointer to the preconditioner.
newton_pc_t* dae_integrator_preconditioner(dae_integrator_t* integrator);

// Sets whether to use a stability limit detection algorithm to improve 
// robustness on particularly stiff problems (2-10% overhead, depending 
// on the problem).
void dae_integrator_set_stability_limit_detection(dae_integrator_t* integrator,
                                                  bool use_detection);

// Sets the relative and absolute tolerances for integrated quantities.
void dae_integrator_set_tolerances(dae_integrator_t* integrator,
                                   real_t relative_tol, real_t absolute_tol);

// Sets the error weight function for evaluating the WRMS norm that is used 
// as a proxy for the quality of the solution. This may be used in lieu of 
// relative and absolute tolerances.
void dae_integrator_set_error_weight_function(dae_integrator_t* integrator,
                                              dae_integrator_error_weight_func compute_weights);                               

// Evaluates the right-hand side of the system at the given time and with the 
// given solution U, placing the results in rhs.
void dae_integrator_eval_rhs(dae_integrator_t* integ, real_t t, real_t* U, real_t* rhs);

// Sets the maximum time step size for the next integration step.
void dae_integrator_set_max_dt(dae_integrator_t* integ, real_t max_dt);

// Sets the time past which the integrator will not step.
void dae_integrator_set_stop_time(dae_integrator_t* integ, real_t stop_time);

// Integrates the given solution U in place (given its time derivative U_dot), 
// taking a single step starting at time *t and storing the new time in *t as 
// well. Also evolves U_dot. Returns true if the step succeeded, false if it 
// failed for some reason. If a step fails, t, U, and U_dot remain unchanged.
bool dae_integrator_step(dae_integrator_t* integrator, real_t max_dt, real_t* t, real_t* U, real_t* U_dot);

// Resets the integrator to prepare it to take a step when U, U_dot, and/or t 
// have changed by some process outside of the integrator. This resets any 
// history information stored within the integrator. Values of U are corrected
// in order to be made consistent with the given values of U_dot.
// If correct_initial_conditions is true, the integrator will attempt to 
// correct U given U_dot.
void dae_integrator_reset(dae_integrator_t* integrator, 
                          real_t t, real_t* U, real_t* U_dot,
                          bool correct_initial_conditions);

// Diagnostics for the time integrator.
typedef struct
{
  char* status_message; // borrowed pointer from integrator: do not free.
  long int num_steps;
  int order_of_last_step;
  real_t initial_step_size;
  real_t last_step_size;
  long int num_residual_evaluations;
  long int num_linear_solve_setups;
  long int num_linear_solve_iterations;
  long int num_linear_solve_convergence_failures;
  long int num_error_test_failures;
  long int num_nonlinear_solve_iterations;
  long int num_nonlinear_solve_convergence_failures;
  long int num_preconditioner_evaluations;
  long int num_preconditioner_solves;
} dae_integrator_diagnostics_t;

// Retrieve diagnostics for the time integrator.
void dae_integrator_get_diagnostics(dae_integrator_t* integrator, 
                                    dae_integrator_diagnostics_t* diagnostics);

// Writes time integrator diagnostics to the given file.
void dae_integrator_diagnostics_fprintf(dae_integrator_diagnostics_t* diagnostics, 
                                        FILE* stream);

#endif

