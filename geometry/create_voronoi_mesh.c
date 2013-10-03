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

// This implementation of the Voronoi tessellator uses Tetgen. It is not built 
// unless a Tetgen tarball was found in the 3rdparty/ directory, in which 
// case the polytope library was built. Please see the license file in the 
// Tetgen tarball for license information on Tetgen.

#ifndef HAVE_POLYTOPE
#error "create_voronoi_mesh.c should not be built without polytope!"
#endif

#undef HAVE_HDF5 // FIXME (HACK): Avoids warnings with polytope 
#include "polytope_c.h"

#include "core/table.h"
#include "core/unordered_map.h"
#include "core/unordered_set.h"
#include "core/slist.h"
#include "core/edit_mesh.h"
#include "core/kd_tree.h"
#include "geometry/create_voronoi_mesh.h"

static mesh_t* mesh_from_tessellation(polytope_tessellation_t* tess, 
                                      point_t* generators, 
                                      int_slist_t* deleted_cells)
{
  // Compute the number of edges, which we aren't given by polytope.
  int_table_t* edge_for_nodes = int_table_new();
  int num_edges = 0;

  // Create the mesh.
  mesh_t* mesh = mesh_new(tess->num_cells, 0, // ???
                          tess->num_faces,
                          num_edges,
                          tess->num_nodes);
  
  // Copy node coordinates.
  for (int i = 0; i < mesh->num_nodes; ++i)
  {
    mesh->nodes[i].x = tess->nodes[3*i];
    mesh->nodes[i].y = tess->nodes[3*i+1];
    mesh->nodes[i].z = tess->nodes[3*i+2];
  }

  // Edge <-> node connectivity.
  {
    int_table_cell_pos_t pos = int_table_start(edge_for_nodes);
    int n1, n2, e;
    while (int_table_next_cell(edge_for_nodes, &pos, &n1, &n2, &e))
    {
      mesh->edges[e].node1 = &mesh->nodes[n1];
      ASSERT(n2 >= 0); // FIXME: Not yet enforced.
      mesh->edges[e].node2 = &mesh->nodes[n2];
    }
  }

  // Face <-> edge connectivity.
  for (int f = 0; f < mesh->num_faces; ++f)
  {
    int Ne = tess->face_offsets[f+1] - tess->face_offsets[f];
    for (int e = 0; e < Ne; ++e)
    {
      int offset = tess->face_offsets[f];
      int n1 = (int)tess->face_nodes[offset+e];
      int n2 = (int)tess->face_nodes[offset+(e+1)%Ne];
      int edge_id = *int_table_get(edge_for_nodes, n1, n2);
      mesh_attach_edge_to_face(mesh, &mesh->edges[edge_id], &mesh->faces[f]);
    }
    ASSERT(mesh->faces[f].num_edges == Ne);
  }

  // Cell <-> face connectivity.
  for (int i = 0; i < mesh->num_cells; ++i)
  {
    int Nf = tess->cell_offsets[i+1] - tess->cell_offsets[i];
    for (int f = 0; f < Nf; ++f)
    {
      int offset = tess->cell_offsets[f];
      int face_index = tess->cell_faces[offset+f];
      face_t* face = &mesh->faces[face_index];
      mesh_attach_face_to_cell(mesh, face, &mesh->cells[i]);
    }
  }

  // Compute the normal vectors on faces.
  for (int f = 0; f < mesh->num_faces; ++f)
  {
    face_t* face = &mesh->faces[f];
    int cell1_index = face->cell1 - &mesh->cells[0];
    if (face->cell2 != NULL)
    {
      int cell2_index = face->cell2 - &mesh->cells[0];
      point_displacement(&generators[cell1_index], &generators[cell2_index], &face->normal);
    }
    else
    {
      point_displacement(&generators[cell1_index], &face->center, &face->normal);
    }
  }

  // Compute the mesh's geometry.
  mesh_compute_geometry(mesh);

  // Clean up.
  int_table_free(edge_for_nodes);

  return mesh;
}

mesh_t* create_voronoi_mesh(point_t* generators, int num_generators, 
                            point_t* ghost_generators, int num_ghost_generators,
                            int_slist_t* deleted_cells)
{
  ASSERT(generators != NULL);
  ASSERT(num_generators >= 2);
  ASSERT(num_ghost_generators >= 0);

  if (deleted_cells != NULL)
    int_slist_clear(deleted_cells);

  // Gather the points to be tessellated.
  int num_points = num_generators + num_ghost_generators;
  double* points = malloc(sizeof(point_t) * 3 * num_points);
  for (int i = 0; i < num_generators; ++i)
  {
    points[3*i] = generators[i].x;
    points[3*i+1] = generators[i].y;
    points[3*i+2] = generators[i].z;
  }
  for (int i = 0; i < num_ghost_generators; ++i)
  {
    points[3*(num_generators+i)] = ghost_generators[i].x;
    points[3*(num_generators+i)+1] = ghost_generators[i].y;
    points[3*(num_generators+i)+2] = ghost_generators[i].z;
  }

  // Perform an unbounded tessellation using polytope.
  polytope_tessellator_t* tessellator = tetgen_tessellator_new();
  polytope_tessellation_t* tess = polytope_tessellation_new(3);
  polytope_tessellator_tessellate_unbounded(tessellator, points, num_points, tess);

  // Create a Voronoi mesh from this tessellation, deleting unbounded cells.
  // FIXME: This doesn't accommodate ghost generators.
  mesh_t* mesh = mesh_from_tessellation(tess, generators, deleted_cells);

  // Clean up.
  polytope_tessellation_free(tess);
  polytope_tessellator_free(tessellator);
  free(points);

  // Stick the generators into a point set (kd-tree) that the mesh can 
  // carry with it.
  kd_tree_t* generator_set = kd_tree_new(generators, num_generators);
  mesh_set_property(mesh, "generators", generator_set, DTOR(kd_tree_free));

  return mesh;
}

