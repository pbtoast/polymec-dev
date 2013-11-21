// Copyright 2012-2013 Jeffrey Johnson.
// 
// This file is part of Polymec, and is licensed under the Apache License, 
// Version 2.0 (the "License"); you may not use this file except in 
// compliance with the License. You may may find the text of the license in 
// the LICENSE file at the top-level source directory, or obtain a copy of 
// it at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "integrators/integrator.h"
#include "core/sundials_helpers.h"
#include "cvode/cvode_spils.h"
#include "cvode/cvode_spgmr.h"
#include "cvode/cvode_spbcgs.h"

struct integrator_t 
{
  char* name;
  void* context;
  void (*dtor)(void*);
  adj_graph_t* graph;
  int order;
  integrator_solver_type_t solver_type;
};

integrator_t* integrator_new(const char* name, 
                             void* context,
                             CVRhsFn rhs,
                             void (*dtor)(void*),
                             adj_graph_t* graph,
                             int order,
                             integrator_solver_type_t solver_type)
{
  ASSERT(rhs != NULL);
  ASSERT(order > 0);
  integrator_t* integ = malloc(sizeof(integrator_t));
  integ->name = string_dup(name);
  integ->context = context;
  integ->graph = graph;
  integ->order = order;
  integ->solver_type = solver_type;
  return integ;
}

void integrator_free(integrator_t* integrator)
{
  if ((integrator->context != NULL) && (integrator->dtor != NULL))
    integrator->dtor(integrator->context);
  free(integrator->name);
  free(integrator);
}

char* integrator_name(integrator_t* integrator)
{
  return integrator->name;
}

void* integrator_context(integrator_t* integrator)
{
  return integrator->context;
}

int integrator_order(integrator_t* integrator)
{
  return integrator->order;
}

void integrator_step(integrator_t* integrator, double t1, double t2, double* X)
{
  ASSERT(t2 > t1);
  // FIXME
}

