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
#include "integrators/ark_ode_integrator.h"

extern ode_integrator_t* functional_ark_diurnal_integrator_new();
extern ode_integrator_t* block_jacobi_precond_ark_diurnal_integrator_new();
extern ode_integrator_t* lu_precond_ark_diurnal_integrator_new();
extern ode_integrator_t* ilu_precond_ark_diurnal_integrator_new();
extern real_t* diurnal_initial_conditions(ode_integrator_t* integ);

void test_functional_diurnal_ctor(void** state)
{
  ode_integrator_t* integ = functional_ark_diurnal_integrator_new();
  ode_integrator_free(integ);
}

void test_block_jacobi_precond_diurnal_ctor(void** state)
{
  ode_integrator_t* integ = block_jacobi_precond_ark_diurnal_integrator_new();
  ode_integrator_free(integ);
}

void test_lu_precond_diurnal_ctor(void** state)
{
  ode_integrator_t* integ = lu_precond_ark_diurnal_integrator_new();
  ode_integrator_free(integ);
}

void test_ilu_precond_diurnal_ctor(void** state)
{
  ode_integrator_t* integ = ilu_precond_ark_diurnal_integrator_new();
  ode_integrator_free(integ);
}

void test_diurnal_step(void** state, ode_integrator_t* integ, real_t max_dt)
{
  // Set up the problem.
  ark_ode_integrator_set_tolerances(integ, 1e-5, 1e-3);
  real_t* u = diurnal_initial_conditions(integ);

  // Integrate it.
  real_t t = 0.0;
  while (t < 7200.0)
  {
    bool integrated = ode_integrator_step(integ, MIN(7200.0, max_dt), &t, u);
//    preconditioner_matrix_fprintf(ode_integrator_preconditioner_matrix(integ), stdout);
    assert_true(integrated);
  }
//printf("u = [");
//for (int i = 0; i < 200; ++i)
//printf("%g ", u[i]);
//printf("]\n");
  ark_ode_integrator_diagnostics_t diags;
  ark_ode_integrator_get_diagnostics(integ, &diags);
  ark_ode_integrator_diagnostics_fprintf(&diags, stdout);

  ode_integrator_free(integ);
  free(u);
}

void test_functional_diurnal_step(void** state)
{
  ode_integrator_t* integ = functional_ark_diurnal_integrator_new();
  test_diurnal_step(state, integ, 1.0);
}

void test_block_jacobi_precond_diurnal_step(void** state)
{
  ode_integrator_t* integ = block_jacobi_precond_ark_diurnal_integrator_new();
  test_diurnal_step(state, integ, FLT_MAX);
}

void test_lu_precond_diurnal_step(void** state)
{
  ode_integrator_t* integ = lu_precond_ark_diurnal_integrator_new();
  test_diurnal_step(state, integ, FLT_MAX);
}

void test_ilu_precond_diurnal_step(void** state)
{
  ode_integrator_t* integ = ilu_precond_ark_diurnal_integrator_new();
  test_diurnal_step(state, integ, FLT_MAX);
}

int main(int argc, char* argv[]) 
{
  polymec_init(argc, argv);
  const UnitTest tests[] = 
  {
    unit_test(test_functional_diurnal_ctor),
    unit_test(test_block_jacobi_precond_diurnal_ctor),
    unit_test(test_lu_precond_diurnal_ctor),
    unit_test(test_ilu_precond_diurnal_ctor),
    unit_test(test_functional_diurnal_step),
    unit_test(test_block_jacobi_precond_diurnal_step),
    unit_test(test_lu_precond_diurnal_step),
    unit_test(test_ilu_precond_diurnal_step)
  };
  return run_tests(tests);
}
