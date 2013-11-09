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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include "cmockery.h"
#include "core/write_silo.h"
#include "geometry/create_rectilinear_mesh.h"
#include "polytope_c.h"

void test_create_rectilinear_mesh(void** state)
{
  // Create a 10x10x10 rectilinear mesh.
  double xs[] = {0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0};
  double ys[] = {0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0};
  double zs[] = {0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0};
  mesh_t* mesh = create_rectilinear_mesh(MPI_COMM_WORLD, xs, 11, ys, 11, zs, 11);
  mesh_verify(mesh);
  assert_int_equal(10*10*10, mesh->num_cells);
  assert_int_equal(0, mesh->num_ghost_cells);
  assert_int_equal(mesh->num_faces, 3*10*10*11);
  assert_int_equal(mesh->num_edges, 3*10*11*11);
  assert_int_equal(11*11*11, mesh->num_nodes);
  mesh_free(mesh);
}

void test_plot_rectilinear_mesh(void** state)
{
  // Create a 4x4x4 rectilinear mesh.
  double xs[] = {0.0, 1.0, 2.0, 4.0, 8.0};
  double ys[] = {0.0, 1.0, 2.0, 4.0, 8.0};
  double zs[] = {0.0, 1.0, 2.0, 4.0, 8.0};
  mesh_t* mesh = create_rectilinear_mesh(MPI_COMM_WORLD, xs, 5, ys, 5, zs, 5);

  // Plot it.
  double ones[4*4*4];
  for (int c = 0; c < 4*4*4; ++c)
    ones[c] = 1.0*c;
  string_ptr_unordered_map_t* fields = string_ptr_unordered_map_new();
  string_ptr_unordered_map_insert(fields, "solution", ones);
  write_silo_mesh(mesh, NULL, NULL, NULL, fields, "rectilinear_4x4x4", ".",
                  0, 0.0, MPI_COMM_SELF, 1, 0);

  // Clean up.
  string_ptr_unordered_map_free(fields);
  mesh_free(mesh);
}

int main(int argc, char* argv[]) 
{
  polymec_init(argc, argv);
  const UnitTest tests[] = 
  {
    unit_test(test_create_rectilinear_mesh),
    unit_test(test_plot_rectilinear_mesh)
  };
  return run_tests(tests);
}
