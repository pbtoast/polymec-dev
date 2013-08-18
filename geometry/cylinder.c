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

#include "geometry/cylinder.h"

typedef struct
{
  vector_t d;
  point_t x;
  double r;
  normal_orient_t orient;
} cyl_t;

static void cyl_eval(void* ctx, point_t* x, double* result)
{
  cyl_t* c = ctx;
  double sign = (c->orient == OUTWARD_NORMAL) ? -1.0 : 1.0;
  double r2 = (x->x - c->x.x)*(x->x - c->x.x) + 
              (x->y - c->x.y)*(x->y - c->x.y);
  result[0] = sign * (sqrt(r2) - c->r);
}

static void cyl_eval_gradient(void* ctx, point_t* x, double* result)
{
  cyl_t* c = ctx;
  double r2 = (x->x - c->x.x)*(x->x - c->x.x) + 
              (x->y - c->x.y)*(x->y - c->x.y);
  double D = fabs(sqrt(r2) - c->r);
  if (D == 0.0)
    result[0] = result[1] = result[2] = 0.0;
  else
  {
    result[0] = (x->x - c->x.x) / D;
    result[1] = (x->y - c->x.y) / D;
    result[2] = 0.0;
  }
}

sp_func_t* cylinder_new(point_t* x, double r, normal_orient_t normal_orientation)
{
  // Set up a cylinder signed distance function.
  cyl_t* c = malloc(sizeof(cyl_t));
  point_copy(&c->x, x);
  c->r = r;
  c->orient = normal_orientation;

  char cyl_str[1024];
  sprintf(cyl_str, "Cylinder (x = (%g %g %g), r = %g)", 
          x->x, x->y, x->z, r);
  sp_vtable vtable = {.eval = cyl_eval, .dtor = free};
  sp_func_t* cyl = sp_func_new(cyl_str, c, vtable, SP_INHOMOGENEOUS, 1);

  // Register the gradient function.
  char cyl_grad_str[1024];
  sprintf(cyl_grad_str, "Cylinder gradient (x = (%g %g %g), r = %g)", 
          x->x, x->y, x->z, r);
  sp_vtable vtable_g = {.eval = cyl_eval_gradient}; // Notice no dtor.
  sp_func_t* cyl_grad = sp_func_new(cyl_grad_str, c, vtable_g, SP_INHOMOGENEOUS, 3);
  sp_func_register_deriv(cyl, 1, cyl_grad);

  return cyl;
}

