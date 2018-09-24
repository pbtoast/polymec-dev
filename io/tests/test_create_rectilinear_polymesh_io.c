// Copyright (c) 2012-2018, Jeffrey N. Johnson
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
#include "core/array_utils.h"
#include "geometry/create_rectilinear_polymesh.h"
#include "io/silo_file.h"

static void test_plot_rectilinear_mesh(void** state)
{
  // Create a 4x4x4 rectilinear mesh.
  real_t xs[] = {0.0, 1.0, 2.0, 4.0, 8.0};
  real_t ys[] = {0.0, 1.0, 2.0, 4.0, 8.0};
  real_t zs[] = {0.0, 1.0, 2.0, 4.0, 8.0};
  polymesh_t* mesh = create_rectilinear_polymesh(MPI_COMM_WORLD, xs, 5, ys, 5, zs, 5);

  // Plot it.
  polymesh_field_t* cfield = polymesh_field_new(mesh, POLYMESH_CELL, 1);
  DECLARE_POLYMESH_FIELD_ARRAY(cvals, ones);
  const char* cnames[] = {"solution"};
  for (int c = 0; c < 4*4*4; ++c)
    cvals[c][0] = 1.0*c;
  silo_file_t* silo = silo_file_new(MPI_COMM_WORLD, "rectilinear_4x4x4", "", 1, 0, 0.0);
  silo_file_write_polymesh(silo, "mesh", mesh);
  silo_field_metadata_t* metadata = silo_field_metadata_new();
  metadata->label = string_dup("solution");
  metadata->units = string_dup("quatloo");
  metadata->conserved = true;
  metadata->extensive = false;
  metadata->vector_component = 2;
  silo_file_write_polymesh_field(silo, cnames, "mesh", cfield, metadata);

  // Add some fields with different centerings.
  polymesh_field_t* nfield = polymesh_field_new(mesh, POLYMESH_NODE, 3);
  DECLARE_POLYMESH_FIELD_ARRAY(nvals, nfield);
  const char* nnames[] = {"nx", "ny", "nz"};
  for (int n = 0; n < mesh->num_nodes; ++n)
  {
    nvals[n][0] = mesh->nodes[n].x;
    nvals[n][1] = mesh->nodes[n].y;
    nvals[n][2] = mesh->nodes[n].z;
  }
  silo_file_write_polymesh_field(silo, nnames, "mesh", nfield, NULL);
  
  polymesh_field_t* ffield = polymesh_field_new(mesh, POLYMESH_FACE, 1);
  DECLARE_POLYMESH_FIELD_ARRAY(fvals, ffield);
  const char* fnames[] = {"fvals"};
  for (int f = 0; f < mesh->num_faces; ++f)
    fvals[f][0] = 1.0 * f;
  silo_file_write_polymesh_field(silo, fnames, "mesh", ffield, NULL);

  polymesh_field_t* efield = polymesh_field_new(mesh, POLYMESH_EDGE, 1);
  DECLARE_POLYMESH_FIELD_ARRAY(evals, efield);
  const char* enames[] = {"evals"};
  for (int e = 0; e < mesh->num_edges; ++e)
    evals[e][0] = 1.0 * e;
  silo_file_write_polymesh_field(silo, enames, "mesh", efield, NULL);
  silo_file_close(silo);

  // Now read the mesh from the file.
  metadata = silo_field_metadata_new();
  real_t t;
  silo = silo_file_open(MPI_COMM_WORLD, "rectilinear_4x4x4", "", 0, &t);
  assert_true(reals_equal(t, 0.0));
  assert_true(silo_file_contains_polymesh(silo, "mesh"));
  mesh1 = silo_file_read_polymesh(silo, "mesh");
  assert_true(mesh1->num_cells <= 4*4*4);

  // Check on the fields.

  // cell field
  assert_true(silo_file_contains_polymesh_field(silo, "solution", "mesh", POLYMESH_CELL));
  polymesh_field_t* cfield1 = polymesh_field_new(mesh, POLYMESH_CELL, 1);
  silo_file_read_polymesh_field(silo, cnames, "mesh", cfield1, metadata);
  assert_true(polymesh_field_compare_all(cfield1, cfield, 0, reals_equal));

  // cell field metadata
  assert_int_equal(0, strcmp(metadata->label, "solution"));
  assert_int_equal(0, strcmp(metadata->units, "quatloo"));
  assert_true(metadata->conserved);
  assert_false(metadata->extensive);
  assert_int_equal(2, metadata->vector_component);
  polymec_release(metadata);

  // node field
  assert_true(silo_file_contains_polymesh_field(silo, "nx", "mesh", POLYMESH_NODE));
  assert_true(silo_file_contains_polymesh_field(silo, "ny", "mesh", POLYMESH_NODE));
  assert_true(silo_file_contains_polymesh_field(silo, "nz", "mesh", POLYMESH_NODE));
  polymesh_field_t* nfield1 = polymesh_field_new(mesh, POLYMESH_NODE, 3);
  silo_file_read_polymesh_field(silo, nnames, "mesh", nfield1, NULL);
  assert_true(polymesh_field_compare_all(nfield1, nfield, 0, reals_equal));
  assert_true(polymesh_field_compare_all(nfield1, nfield, 1, reals_equal));
  assert_true(polymesh_field_compare_all(nfield1, nfield, 2, reals_equal));

  // face field
  assert_true(silo_file_contains_polymesh_field(silo, "fvals", "mesh", POLYMESH_FACE));
  polymesh_field_t* ffield1 = polymesh_field_new(mesh, POLYMESH_FACE, 1);
  silo_file_read_polymesh_field(silo, fnames, "mesh", ffield1, NULL);
  assert_true(polymesh_field_compare_all(ffield1, ffield, 0, reals_equal));

  // edge field
  assert_true(silo_file_contains_polymesh_field(silo, "evals", "mesh", POLYMESH_EDGE));
  polymesh_field_t* efield1 = polymesh_field_new(mesh, POLYMESH_EDGE, 1);
  assert_true(polymesh_field_compare_all(efield1, efield, 0, reals_equal));
  silo_file_close(silo);

  // Clean up.
  polymesh_field_free(cfield1);
  polymesh_field_free(cfield);
  polymesh_field_free(ffield1);
  polymesh_field_free(ffield);
  polymesh_field_free(efield1);
  polymesh_field_free(efield);
  polymesh_field_free(nfield1);
  polymesh_field_free(nfield);
  polymesh_free(mesh1);
  polymesh_free(mesh);
}

static void check_cell_face_connectivity(void** state, 
                                         polymesh_t* mesh,
                                         index_t* global_cell_indices, 
                                         index_t global_cell_index, 
                                         int cell_face_index, 
                                         index_t global_opp_cell_index)
{
  ASSERT(cell_face_index >= 0);
  ASSERT(cell_face_index < 6);

  // Find the given global cell index within our array.
  int cell_array_len = mesh->num_cells + mesh->num_ghost_cells;
  index_t* cell_p = index_lsearch(global_cell_indices, cell_array_len, global_cell_index);
  assert_true(cell_p != NULL);
  int cell = (int)(cell_p - global_cell_indices);
  int face = mesh->cell_faces[6*cell + cell_face_index];
  if (face < 0) 
    face = ~face;
  int opp_cell = polymesh_face_opp_cell(mesh, face, cell);
  if ((opp_cell == -1) && (global_opp_cell_index == (index_t)(-1)))
    assert_int_equal(global_opp_cell_index, opp_cell);
  else
    assert_int_equal(global_opp_cell_index, global_cell_indices[opp_cell]); 
}

int main(int argc, char* argv[]) 
{
  polymec_init(argc, argv);
  silo_enable_compression(1); // create compressed files.
  set_log_level(LOG_DEBUG);
  const struct CMUnitTest tests[] = 
  {
    cmocka_unit_test(test_plot_rectilinear_mesh)
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
