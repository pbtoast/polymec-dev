// Copyright (c) 2012-2018, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This file implements unimesh-related methods for the silo_file class.

#include "core/polymec.h"

#include "silo.h"
#include "core/arch.h"
#include "core/logging.h"
#include "core/array.h"
#include "core/array_utils.h"
#include "core/timer.h"
#include "io/silo_file.h"

#if POLYMEC_HAVE_DOUBLE_PRECISION
#define SILO_FLOAT_TYPE DB_DOUBLE
#else
#define SILO_FLOAT_TYPE DB_FLOAT
#endif

// These functions are implemented in polymec/core/silo_file.c, and used 
// here, even though they are not part of polymec's API.
extern MPI_Comm silo_file_comm(silo_file_t* file);
extern DBfile* silo_file_dbfile(silo_file_t* file);
extern DBoptlist* optlist_from_metadata(silo_field_metadata_t* metadata);
extern void optlist_free(DBoptlist* optlist);
extern void silo_file_add_subdomain_mesh(silo_file_t* file, const char* mesh_name, int silo_mesh_type, DBoptlist* optlist);
extern void silo_file_add_subdomain_field(silo_file_t* file, const char* mesh_name, const char* field_name, int silo_field_type, DBoptlist* optlist);

static void write_unimesh_patch_grid(silo_file_t* file,
                                     const char* patch_grid_name,
                                     int N1, int N2, int N3, // index dimensions of containing space, if any
                                     int i1, int i2, int j1, int j2, int k1, int k2, // bounds of patch grid
                                     bbox_t* bbox,
                                     coord_mapping_t* mapping,
                                     bool hide_from_gui)
{
  ASSERT(i2 > i1);
  ASSERT(j2 > j1);
  ASSERT(k2 > k1);

  DBfile* dbfile = silo_file_dbfile(file);

  // Name the coordinate axes.
  const char* const coord_names[3] = {"x1", "x2", "x3"};

  // Provide coordinates.
  int n1 = i1 + i2, n2 = j1 + j2, n3 = k1 + k2;
  ASSERT(n1 > 0);
  ASSERT(n2 > 0);
  ASSERT(n3 > 0);
  int N = (n1+1) * (n2+1) * (n3+1);
  real_t* x1_node = polymec_malloc(sizeof(real_t) * N);
  real_t* x2_node = polymec_malloc(sizeof(real_t) * N);
  real_t* x3_node = polymec_malloc(sizeof(real_t) * N);
  int dimensions[3] = {n1+1, n2+1, n3+1};
  int coord_type;
  if (mapping != NULL)
  {
    coord_type = DB_NONCOLLINEAR;
    real_t dx1 = 1.0 / N1, dx2 = 1.0 / N2, dx3 = 1.0 / N3;
    point_t x;
    int l = 0;
    for (int i = i1; i <= i2; ++i)
    {
      x.x = i * dx1;
      for (int j = j1; j <= j2; ++j)
      {
        x.y = j * dx2;
        for (int k = k1; k <= k2; ++k, ++l)
        {
          x.z = k * dx3;
          point_t y;
          coord_mapping_map_point(mapping, &x, &y);
          x1_node[l] = y.x;
          x2_node[l] = y.y;
          x3_node[l] = y.z;
        }
      }
    }
  }
  else
  {
    coord_type = DB_COLLINEAR;
    real_t dx = 1.0 / N1;
    for (int i = i1; i <= i2; ++i)
      x1_node[i-i1] = i*dx;
    real_t dy = 1.0 / N2;
    for (int j = j1; j <= j2; ++j)
      x2_node[j-j1] = j*dy;
    real_t dz = 1.0 / N3;
    for (int k = k1; k <= k2; ++k)
      x3_node[k-k1] = k*dz;
  }

  real_t* coords[3];
  coords[0] = x1_node;
  coords[1] = x2_node;
  coords[2] = x3_node;

  // Write the patch grid.
  int num_options = (hide_from_gui) ? 3 : 2;
  DBoptlist* optlist = DBMakeOptlist(num_options); 
  int lo_offsets[3] = {i1, j1, k1};
  DBAddOption(optlist, DBOPT_LO_OFFSET, lo_offsets);
  int hi_offsets[3] = {i1, j1, k1};
  DBAddOption(optlist, DBOPT_HI_OFFSET, hi_offsets);
  if (hide_from_gui)
  {
    int one = 1;
    DBAddOption(optlist, DBOPT_HIDE_FROM_GUI, &one);
  }
  DBPutQuadmesh(dbfile, patch_grid_name, coord_names, coords, dimensions, 3, 
                SILO_FLOAT_TYPE, coord_type, optlist);
  DBFreeOptlist(optlist);
  polymec_free(x1_node);
  polymec_free(x2_node);
  polymec_free(x3_node);
}

void silo_file_write_unimesh(silo_file_t* file, 
                             const char* mesh_name,
                             unimesh_t* mesh,
                             coord_mapping_t* mapping)
{
  START_FUNCTION_TIMER();
  DBfile* dbfile = silo_file_dbfile(file);

  // This mesh is really just a grouping of patches, as far as SILO is 
  // concerned.
  DBSetDir(dbfile, "/");
  int num_local_patches = unimesh_num_patches(mesh);
  int npx, npy, npz, nx, ny, nz;
  unimesh_get_extents(mesh, &npx, &npy, &npz);
  unimesh_get_patch_size(mesh, &nx, &ny, &nz);

  // Write the bounding box.
  {
    char bbox_name[FILENAME_MAX+1];
    snprintf(bbox_name, FILENAME_MAX, "%s_bbox", mesh_name);
    bbox_t* bbox = unimesh_bbox(mesh);
    silo_file_write_real_array(file, bbox_name, (real_t*)bbox, 6);
  }

  // Write extent and patch size information for this mesh.
  {
    char array_name[FILENAME_MAX+1];
    snprintf(array_name, FILENAME_MAX, "%s_extents", mesh_name);
    int extents[3] = {npx, npy, npz};
    silo_file_write_int_array(file, array_name, extents, 3);
    snprintf(array_name, FILENAME_MAX, "%s_patch_sizes", mesh_name);
    int patch_sizes[3] = {nx, ny, nz};
    silo_file_write_int_array(file, array_name, patch_sizes, 3);
  }

  char* patch_grid_names[num_local_patches];
  int patch_grid_types[num_local_patches];
  int N1 = npx * nx, N2 = npy * ny, N3 = npz * nz;
  int pos = 0, i, j, k, l = 0;
  int_array_t* patch_indices = int_array_new();
  bbox_t bbox;
  while (unimesh_next_patch(mesh, &pos, &i, &j, &k, &bbox)) 
  {
    // Write out the grid for the patch itself.
    int i1 = nx*i, i2 = nx*(i+1), j1 = ny*j, j2 = ny*(j+1), k1 = nz*k, k2 = nz*(k+1);
    char patch_grid_name[FILENAME_MAX+1];
    snprintf(patch_grid_name, FILENAME_MAX, "%s_%d_%d_%d", mesh_name, i, j, k);
    patch_grid_names[l] = string_dup(patch_grid_name);
    patch_grid_types[l] = DB_QUAD_RECT;
    write_unimesh_patch_grid(file, patch_grid_names[l], N1, N2, N3, 
                                                    i1, i2, j1, j2, k1, k2, 
                                                    &bbox, mapping, true);

    // Jot down this (i, j, k) triple.
    int_array_append(patch_indices, i);
    int_array_append(patch_indices, j);
    int_array_append(patch_indices, k);

    ++l;
  }
  ASSERT(l == num_local_patches);

  // Record the indices of the patches in the mesh.
  {
    char array_name[FILENAME_MAX+1];
    snprintf(array_name, FILENAME_MAX, "%s_patch_indices", mesh_name);
    silo_file_write_int_array(file, array_name, patch_indices->data, patch_indices->size);
    int_array_free(patch_indices);
  }

  // Group all the patches together.
  DBPutMultimesh(dbfile, mesh_name, num_local_patches, (const char* const*)patch_grid_names, patch_grid_types, NULL);

  // Clean up.
  for (int p = 0; p < num_local_patches; ++p)
    string_free(patch_grid_names[p]);

  // Add subdomain information for this mesh.
  silo_file_add_subdomain_mesh(file, mesh_name, DB_QUAD_RECT, NULL);
  STOP_FUNCTION_TIMER();
}

static void read_unimesh_extent_and_patch_size(silo_file_t* file,
                                               const char* mesh_name,
                                               int* npx, int* npy, int* npz,
                                               int* nx, int* ny, int* nz)
{
  char array_name[FILENAME_MAX+1];
  snprintf(array_name, FILENAME_MAX, "%s_extents", mesh_name);
  size_t size;
  int* extents = silo_file_read_int_array(file, array_name, &size);
  if (size != 3)
    polymec_error("silo_file_read_unimesh: Invalid extent data.");
  *npx = extents[0]; 
  *npy = extents[1]; 
  *npz = extents[2];
  polymec_free(extents);

  snprintf(array_name, FILENAME_MAX, "%s_patch_sizes", mesh_name);
  int* patch_sizes = silo_file_read_int_array(file, array_name, &size);
  if (size != 3)
    polymec_error("silo_file_read_unimesh: Invalid patch size data.");
  *nx = patch_sizes[0];
  *ny = patch_sizes[1]; 
  *nz = patch_sizes[2];
  polymec_free(patch_sizes);
}

static void read_unimesh_patch_indices(silo_file_t* file,
                                       const char* mesh_name,
                                       int_array_t* i_array,
                                       int_array_t* j_array,
                                       int_array_t* k_array)
{
  // Clear the arrays.
  int_array_clear(i_array);
  int_array_clear(j_array);
  int_array_clear(k_array);

  char array_name[FILENAME_MAX+1];
  snprintf(array_name, FILENAME_MAX, "%s_patch_indices", mesh_name);
  size_t size;
  int* indices = silo_file_read_int_array(file, array_name, &size);
  if ((size % 3) != 0)
    polymec_error("silo_file_read_unimesh: Invalid patch index data.");
  int num_patches = (int)(size/3);
  for (int p = 0; p < num_patches; ++p)
  {
    int_array_append(i_array, indices[3*p]);
    int_array_append(j_array, indices[3*p+1]);
    int_array_append(k_array, indices[3*p+2]);
  }
  polymec_free(indices);
}

unimesh_t* silo_file_read_unimesh(silo_file_t* file, 
                                  const char* mesh_name)
{
  START_FUNCTION_TIMER();
  // Read the bounding box.
  char bbox_name[FILENAME_MAX+1];
  snprintf(bbox_name, FILENAME_MAX, "%s_bbox", mesh_name);
  bbox_t bbox;
  size_t six;
  real_t* bounds = silo_file_read_real_array(file, bbox_name, &six);
  memcpy(&bbox, bounds, sizeof(real_t) * six);
  polymec_free(bounds);

  // Read the extent/patch size information for the mesh.
  int npx, npy, npz, nx, ny, nz;
  read_unimesh_extent_and_patch_size(file, mesh_name, &npx, &npy, &npz, &nx, &ny, &nz);

  // Create the mesh.
#if POLYMEC_HAVE_MPI
  MPI_Comm comm = silo_file_comm(file);
#else
  MPI_Comm comm = MPI_COMM_WORLD;
#endif
  unimesh_t* mesh = create_empty_unimesh(comm, &bbox, npx, npy, npz, nx, ny, nz);

  // Fill it with patches whose indices we read from the file.
  int_array_t* i_array = int_array_new();
  int_array_t* j_array = int_array_new();
  int_array_t* k_array = int_array_new();
  read_unimesh_patch_indices(file, mesh_name, i_array, j_array, k_array);
  for (size_t p = 0; p < i_array->size; ++p)
    unimesh_insert_patch(mesh, i_array->data[p], j_array->data[p], k_array->data[p]);
  int_array_free(i_array);
  int_array_free(j_array);
  int_array_free(k_array);

  unimesh_finalize(mesh);
  STOP_FUNCTION_TIMER();
  return mesh;
}

bool silo_file_contains_unimesh(silo_file_t* file, 
                                const char* mesh_name)
{
  DBfile* dbfile = silo_file_dbfile(file);
  return (DBInqVarExists(dbfile, mesh_name) && 
          (DBInqVarType(dbfile, mesh_name) == DB_MULTIMESH));
}

static void query_unimesh_vector_comps(unimesh_patch_t* patch,
                                       silo_field_metadata_t** field_metadata,
                                       coord_mapping_t* mapping,
                                       bool* is_vector_comp,
                                       int* first_vector_comp)
{
  int num_vector_comps = 0;
  *first_vector_comp = -1;
  if ((mapping != NULL) && (field_metadata != NULL))
  {
    for (int c = 0; c < patch->nc; ++c)
    {
      if ((field_metadata[c] != NULL) && 
          (field_metadata[c]->vector_component != -1))
      {
        if ((num_vector_comps % 3) == 0)
        {
          ASSERT(patch->nc >= c + 3); // If you start a vector, you better finish your vector.
          *first_vector_comp = c;
        }
        is_vector_comp[c] = true;
        ++num_vector_comps;
      }
    }
  }
  else
    memset(is_vector_comp, 0, sizeof(bool) * patch->nc);
  ASSERT((num_vector_comps % 3) == 0); // We should have complete sets of vector triples.
}

static void copy_out_unimesh_node_component(unimesh_patch_t* patch,
                                            silo_field_metadata_t** field_metadata,
                                            int c,
                                            bbox_t* bbox,
                                            coord_mapping_t* mapping,
                                            real_t* data)
{
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  DECLARE_UNIMESH_NODE_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    int l = 0;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i <= patch->nx; ++i)
    {
      x.x = bbox->x1 + i * dx;
      for (int j = 0; j <= patch->ny; ++j)
      {
        x.y = bbox->y1 + j * dy;
        for (int k = 0; k <= patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + k * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    int l = 0;
    for (int i = 0; i <= patch->nx; ++i)
      for (int j = 0; j <= patch->ny; ++j)
        for (int k = 0; k <= patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_xedge_component(unimesh_patch_t* patch,
                                             silo_field_metadata_t** field_metadata,
                                             int c,
                                             bbox_t* bbox,
                                             coord_mapping_t* mapping,
                                             real_t* data)
{
  // FIXME: Needs to preserve existing edge data!
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  DECLARE_UNIMESH_XEDGE_ARRAY(a, patch);
  int l = 0;
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i < patch->nx; ++i)
    {
      x.x = bbox->x1 + (i+0.5) * dx;
      for (int j = 0; j <= patch->ny; ++j)
      {
        x.y = bbox->y1 + j * dy;
        for (int k = 0; k <= patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + k * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    for (int i = 0; i < patch->nx; ++i)
      for (int j = 0; j <= patch->ny; ++j)
        for (int k = 0; k <= patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_yedge_component(unimesh_patch_t* patch,
                                             silo_field_metadata_t** field_metadata,
                                             int c,
                                             bbox_t* bbox,
                                             coord_mapping_t* mapping,
                                             real_t* data)
{
  // FIXME: Needs to preserve existing edge data!
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  int l = (patch->nx+1)*(patch->ny+1)*(patch->nz+1);
  DECLARE_UNIMESH_YEDGE_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i <= patch->nx; ++i)
    {
      x.x = bbox->x1 + i * dx;
      for (int j = 0; j < patch->ny; ++j)
      {
        x.y = bbox->y1 + (j+0.5) * dy;
        for (int k = 0; k <= patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + k * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    for (int i = 0; i <= patch->nx; ++i)
      for (int j = 0; j < patch->ny; ++j)
        for (int k = 0; k <= patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_zedge_component(unimesh_patch_t* patch,
                                             silo_field_metadata_t** field_metadata,
                                             int c,
                                             bbox_t* bbox,
                                             coord_mapping_t* mapping,
                                             real_t* data)
{
  // FIXME: Needs to preserve existing edge data!
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  int l = 2*(patch->nx+1)*(patch->ny+1)*(patch->nz+1);
  DECLARE_UNIMESH_ZEDGE_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i <= patch->nx; ++i)
    {
      x.x = bbox->x1 + i * dx;
      for (int j = 0; j <= patch->ny; ++j)
      {
        x.y = bbox->y1 + j * dy;
        for (int k = 0; k < patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + (k+0.5) * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    for (int i = 0; i <= patch->nx; ++i)
      for (int j = 0; j <= patch->ny; ++j)
        for (int k = 0; k < patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_xface_component(unimesh_patch_t* patch,
                                             silo_field_metadata_t** field_metadata,
                                             int c,
                                             bbox_t* bbox,
                                             coord_mapping_t* mapping,
                                             real_t* data)
{
  // FIXME: Needs to preserve existing face data!
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  int l = 0;
  DECLARE_UNIMESH_XFACE_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i <= patch->nx; ++i)
    {
      x.x = bbox->x1 + i * dx;
      for (int j = 0; j < patch->ny; ++j)
      {
        x.y = bbox->y1 + (j+0.5) * dy;
        for (int k = 0; k < patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + (k+0.5) * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    for (int i = 0; i <= patch->nx; ++i)
      for (int j = 0; j < patch->ny; ++j)
        for (int k = 0; k < patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_yface_component(unimesh_patch_t* patch,
                                             silo_field_metadata_t** field_metadata,
                                             int c,
                                             bbox_t* bbox,
                                             coord_mapping_t* mapping,
                                             real_t* data)
{
  // FIXME: Needs to preserve existing face data!
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  int l = (patch->nx+1)*(patch->ny+1)*(patch->nz+1);
  DECLARE_UNIMESH_YFACE_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i < patch->nx; ++i)
    {
      x.x = bbox->x1 + (i+0.5) * dx;
      for (int j = 0; j <= patch->ny; ++j)
      {
        x.y = bbox->y1 + j * dy;
        for (int k = 0; k < patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + (k+0.5) * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    for (int i = 0; i < patch->nx; ++i)
      for (int j = 0; j <= patch->ny; ++j)
        for (int k = 0; k < patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_zface_component(unimesh_patch_t* patch,
                                             silo_field_metadata_t** field_metadata,
                                             int c,
                                             bbox_t* bbox,
                                             coord_mapping_t* mapping,
                                             real_t* data)
{
  // FIXME: Needs to preserve existing face data!
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  int l = 2*(patch->nx+1)*(patch->ny+1)*(patch->nz+1);
  DECLARE_UNIMESH_ZFACE_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 0; i < patch->nx; ++i)
    {
      x.x = bbox->x1 + (i+0.5) * dx;
      for (int j = 0; j < patch->ny; ++j)
      {
        x.y = bbox->y1 + (j+0.5) * dy;
        for (int k = 0; k <= patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + k * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    for (int i = 0; i < patch->nx; ++i)
      for (int j = 0; j < patch->ny; ++j)
        for (int k = 0; k <= patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void copy_out_unimesh_cell_component(unimesh_patch_t* patch,
                                            silo_field_metadata_t** field_metadata,
                                            int c,
                                            bbox_t* bbox,
                                            coord_mapping_t* mapping,
                                            real_t* data)
{
  // If we're given a mapping and there are vector fields, we need to treat 
  // them specially.
  bool is_vector_comp[patch->nc];
  int first_vector_comp;
  query_unimesh_vector_comps(patch, field_metadata, mapping,
                             is_vector_comp, &first_vector_comp);

  // Now copy the data.
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  if ((mapping != NULL) && is_vector_comp[c])
  {
    // We need to map this vector field before we write it out.
    int c1 = first_vector_comp, 
        c2 = first_vector_comp+1, 
        c3 = first_vector_comp+2;
    int which_component = c - c1;
    int l = 0;
    point_t x;
    real_t dx = (bbox->x2 - bbox->x1)/patch->nx,
           dy = (bbox->y2 - bbox->y1)/patch->ny,
           dz = (bbox->z2 - bbox->z1)/patch->nz;
    for (int i = 1; i <= patch->nx; ++i)
    {
      x.x = bbox->x1 + (i+0.5) * dx;
      for (int j = 1; j <= patch->ny; ++j)
      {
        x.y = bbox->y1 + (j+0.5) * dy;
        for (int k = 1; k <= patch->nz; ++k, ++l)
        {
          x.z = bbox->z1 + (k+0.5) * dz;
          vector_t v = {.x = a[i][j][k][c1], a[i][j][k][c2], a[i][j][k][c3]};
          vector_t v1;
          coord_mapping_map_vector(mapping, &x, &v, &v1);
          switch (which_component)
          {
            case 0: data[l] = v1.x; break;
            case 1: data[l] = v1.y; break;
            default: data[l] = v1.z;
          }
        }
      }
    }
  }
  else
  {
    // Copy the field data verbatim.
    int l = 0;
    for (int i = 1; i <= patch->nx; ++i)
      for (int j = 1; j <= patch->ny; ++j)
        for (int k = 1; k <= patch->nz; ++k, ++l)
          data[l] = a[i][j][k][c];
  }
}

static void write_unimesh_patch_data(silo_file_t* file,
                                     const char** field_component_names,
                                     const char* patch_grid_name,
                                     unimesh_patch_t* patch,
                                     silo_field_metadata_t** field_metadata,
                                     bbox_t* bbox,
                                     coord_mapping_t* mapping)
{
  DBfile* dbfile = silo_file_dbfile(file);

  // Allocate an array to use for writing Silo data.
  int dimensions[3] = {patch->nx+1, patch->ny+1, patch->nz+1};
  size_t data_size = unimesh_patch_data_size(patch->centering, 
                                             patch->nx, patch->ny, patch->nz, 1);
  real_t* data = polymec_malloc(sizeof(real_t) * data_size);
  int centering;

  // Now write each component.
  for (int c = 0; c < patch->nc; ++c)
  {
    DBoptlist* optlist = (field_metadata != NULL) ? optlist_from_metadata(field_metadata[c]) : NULL;
    switch (patch->centering) 
    {
      // Copy the data in the component into our array.
      case UNIMESH_NODE: 
        centering = DB_NODECENT;
        copy_out_unimesh_node_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_XEDGE:
        centering = DB_EDGECENT;
        copy_out_unimesh_xedge_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_YEDGE:
        centering = DB_EDGECENT;
        copy_out_unimesh_yedge_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_ZEDGE:
        centering = DB_EDGECENT;
        copy_out_unimesh_yedge_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_XFACE:
        centering = DB_FACECENT;
        copy_out_unimesh_xface_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_YFACE:
        centering = DB_FACECENT;
        copy_out_unimesh_yface_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_ZFACE:
        centering = DB_FACECENT;
        copy_out_unimesh_yface_component(patch, field_metadata, c, bbox, mapping, data);
        break;
      case UNIMESH_CELL: 
        dimensions[0] = patch->nx;
        dimensions[1] = patch->ny;
        dimensions[2] = patch->nz;
        centering = DB_ZONECENT;
        copy_out_unimesh_cell_component(patch, field_metadata, c, bbox, mapping, data);
    }
    
    // Write the component to the file.
    DBPutQuadvar1(dbfile, field_component_names[c], patch_grid_name, data,
                  dimensions, 3, NULL, 0, SILO_FLOAT_TYPE, centering, optlist);
    optlist_free(optlist);
  }

  // Clean up.
  polymec_free(data);
}

void silo_file_write_unimesh_field(silo_file_t* file, 
                                   const char** field_component_names,
                                   const char* mesh_name,
                                   unimesh_field_t* field,
                                   silo_field_metadata_t** field_metadata,
                                   coord_mapping_t* mapping)
{
  START_FUNCTION_TIMER();
  int num_local_patches = unimesh_field_num_patches(field);
  int num_components = unimesh_field_num_components(field);

  unimesh_t* mesh = unimesh_field_mesh(field);
  int npx, npy, npz, nx, ny, nz;
  unimesh_get_extents(mesh, &npx, &npy, &npz);
  unimesh_get_patch_size(mesh, &nx, &ny, &nz);

  char* field_names[num_components];
  char* multi_field_names[num_components][num_local_patches];
  int multi_field_types[num_local_patches];

  unimesh_patch_t* patch;
  int pos = 0, i, j, k, l = 0;
  bbox_t bbox;
  while (unimesh_field_next_patch(field, &pos, &i, &j, &k, &patch, &bbox))
  {
    // Write out the patch data itself.
    for (int c = 0; c < num_components; ++c)
    {
      char field_name[FILENAME_MAX];
      snprintf(field_name, FILENAME_MAX-1, "%s_%d_%d_%d", field_component_names[c], i, j, k);
      field_names[c] = string_dup(field_name);
      multi_field_names[c][l] = string_dup(field_name);
    }
    char patch_grid_name[FILENAME_MAX];
    snprintf(patch_grid_name, FILENAME_MAX-1, "%s_%d_%d_%d", mesh_name, i, j, k);
    write_unimesh_patch_data(file, (const char**)field_names, patch_grid_name,  
                             patch, field_metadata, &bbox, mapping);
    multi_field_types[l] = DB_QUADVAR;
    ++l;

    for (int c = 0; c < num_components; ++c)
      string_free(field_names[c]);
  }
  ASSERT(l == num_local_patches);

  // Finally, place multi-* entries into the Silo file.
  DBfile* dbfile = silo_file_dbfile(file);
  for (int c = 0; c < num_components; ++c)
  {
    // We need to associate this multi-variable with our multi-mesh.
    DBoptlist* optlist = DBMakeOptlist(4);
    DBAddOption(optlist, DBOPT_MMESH_NAME, (void*)mesh_name);
    DBPutMultivar(dbfile, field_component_names[c], num_local_patches, (const char* const*)multi_field_names[c], multi_field_types, NULL);
    DBFreeOptlist(optlist);

    // Add subdomain information for this component.
    silo_file_add_subdomain_mesh(file, field_component_names[c], DB_QUAD_RECT, NULL);
  }

  // Clean up.
  for (int c = 0; c < num_components; ++c)
    for (int p = 0; p < num_local_patches; ++p)
      string_free(multi_field_names[c][p]);
  STOP_FUNCTION_TIMER();
}

static void copy_in_unimesh_node_component(DBquadvar* var, 
                                           int c, 
                                           unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = 0;
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i <= patch->nx; ++i)
    for (int j = 0; j <= patch->ny; ++j)
      for (int k = 0; k <= patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_xedge_component(DBquadvar* var, 
                                            int c, 
                                            unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = 0;
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i < patch->nx; ++i)
    for (int j = 0; j <= patch->ny; ++j)
      for (int k = 0; k <= patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_yedge_component(DBquadvar* var, 
                                            int c, 
                                            unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = var->dims[0]*var->dims[1]*var->dims[2];
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i <= patch->nx; ++i)
    for (int j = 0; j < patch->ny; ++j)
      for (int k = 0; k <= patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_zedge_component(DBquadvar* var, 
                                            int c, 
                                            unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = 2*var->dims[0]*var->dims[1]*var->dims[2];
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i <= patch->nx; ++i)
    for (int j = 0; j <= patch->ny; ++j)
      for (int k = 0; k < patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_xface_component(DBquadvar* var, 
                                            int c, 
                                            unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = 0;
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i <= patch->nx; ++i)
    for (int j = 0; j < patch->ny; ++j)
      for (int k = 0; k < patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_yface_component(DBquadvar* var, 
                                            int c, 
                                            unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = var->dims[0]*var->dims[1]*var->dims[2];
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i < patch->nx; ++i)
    for (int j = 0; j <= patch->ny; ++j)
      for (int k = 0; k < patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_zface_component(DBquadvar* var, 
                                            int c, 
                                            unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx + 1);
  ASSERT(var->dims[1] == patch->ny + 1);
  ASSERT(var->dims[2] == patch->nz + 1);
  int l = 2*var->dims[0]*var->dims[1]*var->dims[2];
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 0; i < patch->nx; ++i)
    for (int j = 0; j < patch->ny; ++j)
      for (int k = 0; k <= patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void copy_in_unimesh_cell_component(DBquadvar* var, 
                                           int c, 
                                           unimesh_patch_t* patch)
{
  ASSERT(var->dims[0] == patch->nx);
  ASSERT(var->dims[1] == patch->ny);
  ASSERT(var->dims[2] == patch->nz);
  int l = 0;
  real_t* data = (real_t*)var->vals[0];
  DECLARE_UNIMESH_CELL_ARRAY(a, patch);
  for (int i = 1; i <= patch->nx; ++i)
    for (int j = 1; j <= patch->ny; ++j)
      for (int k = 1; k <= patch->nz; ++k, ++l)
        a[i][j][k][c] = data[l];
}

static void read_unimesh_patch_data(silo_file_t* file,
                                    const char** field_component_names,
                                    const char* patch_grid_name,
                                    int num_components,
                                    unimesh_patch_t* patch,
                                    silo_field_metadata_t** field_metadata)
{
  DBfile* dbfile = silo_file_dbfile(file);

  // Fetch each component from the file.
  for (int c = 0; c < num_components; ++c)
  {
    DBquadvar* var = DBGetQuadvar(dbfile, field_component_names[c]);
    ASSERT(var != NULL);
    ASSERT(var->datatype == SILO_FLOAT_TYPE);
    ASSERT(var->nvals == 1);
    switch (patch->centering) 
    {
      // Copy the data in the component into our array.
      case UNIMESH_NODE: 
        copy_in_unimesh_node_component(var, c, patch);
        break;
      case UNIMESH_XEDGE:
        copy_in_unimesh_xedge_component(var, c, patch);
        break;
      case UNIMESH_YEDGE:
        copy_in_unimesh_yedge_component(var, c, patch);
        break;
      case UNIMESH_ZEDGE:
        copy_in_unimesh_yedge_component(var, c, patch);
        break;
      case UNIMESH_XFACE:
        copy_in_unimesh_xface_component(var, c, patch);
        break;
      case UNIMESH_YFACE:
        copy_in_unimesh_yface_component(var, c, patch);
        break;
      case UNIMESH_ZFACE:
        copy_in_unimesh_yface_component(var, c, patch);
        break;
      case UNIMESH_CELL: 
        copy_in_unimesh_cell_component(var, c, patch);
    }

    // Extract metadata.
    if ((field_metadata != NULL) && (field_metadata[c] != NULL))
    {
      field_metadata[c]->label = string_dup(var->label);
      field_metadata[c]->units = string_dup(var->units);
      field_metadata[c]->conserved = (var->conserved != 0);
      field_metadata[c]->extensive = (var->extensive != 0);
    }

    DBFreeQuadvar(var);
  }
}

void silo_file_read_unimesh_field(silo_file_t* file, 
                                  const char** field_component_names,
                                  const char* mesh_name,
                                  unimesh_field_t* field,
                                  silo_field_metadata_t** field_metadata)
{
  START_FUNCTION_TIMER();
  unimesh_patch_t* patch;
  int pos = 0, i, j, k;
  while (unimesh_field_next_patch(field, &pos, &i, &j, &k, &patch, NULL))
  {
    char* field_names[patch->nc];
    for (int c = 0; c < patch->nc; ++c)
    {
      char field_name[FILENAME_MAX+1];
      snprintf(field_name, FILENAME_MAX, "%s_%d_%d_%d", field_component_names[c], i, j, k);
      field_names[c] = string_dup(field_name);
    }
    read_unimesh_patch_data(file, (const char**)field_names, mesh_name, 
                            patch->nc, patch, field_metadata);
    for (int c = 0; c < patch->nc; ++c)
      string_free(field_names[c]);
  }
  STOP_FUNCTION_TIMER();
}

bool silo_file_contains_unimesh_field(silo_file_t* file, 
                                      const char* field_name,
                                      const char* mesh_name)
{
  // FIXME
  DBfile* dbfile = silo_file_dbfile(file);
  return (DBInqVarExists(dbfile, mesh_name) && 
          (DBInqVarType(dbfile, mesh_name) == DB_MULTIMESH));
}

