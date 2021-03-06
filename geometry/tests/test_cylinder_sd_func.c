// Copyright (c) 2012-2019, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include "cmocka.h"
#include "geometry/cylinder_sd_func.h"
#include "generate_octave_script_for_surface.h"

static void test_construct(void** state)
{
  // Create spheres with inward/outward normals.
  point_t origin = {0.0, 0.0, 0.0};
  sd_func_t* c1 = cylinder_sd_func_new(&origin, 1.0, INWARD_NORMAL);
  assert_true(c1 != NULL);
  sd_func_t* c2 = cylinder_sd_func_new(&origin, 1.0, OUTWARD_NORMAL);
  assert_true(c2 != NULL);
}

static void test_plot(void** state)
{
  // Create a text file containing an Octave script that can be run to 
  // visualize this plot.
  point_t origin = {0.0, 0.0, 0.0};
  sd_func_t* c = cylinder_sd_func_new(&origin, 0.5, OUTWARD_NORMAL);
  bbox_t bbox = {.x1 = -1.0, .x2 = 1.0, .y1 = -1.0, .y2 = 1.0, .z1 = -1.0, .z2 = 1.0};
  generate_octave_script_for_surface(c, 20, &bbox, "test_cylinder.m");
}

int main(int argc, char* argv[]) 
{
  polymec_init(argc, argv);
  const struct CMUnitTest tests[] = 
  {
    cmocka_unit_test(test_construct),
    cmocka_unit_test(test_plot)
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
