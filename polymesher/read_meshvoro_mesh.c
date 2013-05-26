// This implements polymesher's capability for reading polyhedral mesh's 
// generated by Matt Freeman's MeshVoro code.

#include <string.h>
#include "core/polymec.h"
#include "core/interpreter.h"
#include "core/mesh.h"
#include "core/slist.h"
#include "core/table.h"
#include "core/unordered_map.h"
#include "core/edit_mesh.h"

// Lua stuff.
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

// This data structure represents an intermediate representation of 
// a face in terms of its vertices (instead of edges).
typedef struct 
{
  // A face consists of a connected set of vertices.
  int num_vertices;
  int* vertex_ids;
} face_with_vertices_t;

// This is used to create face objects.
static face_with_vertices_t* face_with_vertices_new(int num_vertices, int* vertices)
{
  ASSERT(num_vertices >= 3);
  ASSERT(vertices != NULL);
  face_with_vertices_t* face = malloc(sizeof(face_with_vertices_t));
  face->num_vertices = num_vertices;
  face->vertex_ids = malloc(sizeof(int) * num_vertices);
  memcpy(face->vertex_ids, vertices, sizeof(int) * num_vertices);
  return face;
}

// This is used to destroy face objects.
static void destroy_face(face_with_vertices_t* face)
{
  free(face->vertex_ids);
  free(face);
}

// This function finds the face in the given list that best matches the 
// given face (in terms of vertex indices).
face_with_vertices_t* best_matching_face(face_with_vertices_t* face,
                                         face_with_vertices_t** faces,
                                         int num_faces)
{
  face_with_vertices_t* match = NULL;
  // FIXME
  return match;
}

// This data structure represents an intermediate representation of 
// a cell in terms of its faces.
typedef struct 
{
  char* name; // Element name.
  int index; // (final) cell index.
  int num_faces;
  face_with_vertices_t** faces;
  int* neighbor_ids; // Indices of neighboring cells.
} cell_with_faces_t;

// This is used to create cell objects.
static cell_with_faces_t* cell_with_faces_new(char* name, int index, int num_faces)
{
  ASSERT(index >= 0);
  ASSERT(num_faces >= 4);
  cell_with_faces_t* cell = malloc(sizeof(cell_with_faces_t));
  cell->name = strdup(name);
  cell->index = index;
  cell->num_faces = num_faces;
  cell->faces = malloc(sizeof(face_with_vertices_t*)*num_faces);
  cell->neighbor_ids = malloc(sizeof(int)*num_faces);
  for (int f = 0; f < num_faces; ++f)
  {
    cell->faces[f] = NULL;
    cell->neighbor_ids[f] = -1;
  }
  return cell;
}

// This is used to destroy cell objects.
static void destroy_cell(cell_with_faces_t* cell)
{
  free(cell->name);
  for (int f = 0; f < cell->num_faces; ++f)
    destroy_face(cell->faces[f]);
  free(cell->neighbor_ids);
  free(cell->faces);
  free(cell);
}

// This is used to cleanly destroy linked lists of cells.
static void cell_list_dtor(void* datum)
{
  cell_with_faces_t* cell = datum;
  destroy_cell(cell);
}

static point_t* read_mesh_vertices(FILE* vertex_file, 
                                   int* num_vertices, 
                                   char* error_string)
{
  // Read the points into a linked list.
  int status;
  ptr_slist_t* points_list = ptr_slist_new();
  int last_index = -1;
  do 
  {
    int i;
    double x, y, z;
    status = fscanf(vertex_file, "  %d :  %lE %lE %lE\n", &i, &x, &y, &z);
    if (status != EOF)
    {
      if (status != 4)
      {
        sprintf(error_string, "Error reading line %d of vertex file.", last_index + 1);
        ptr_slist_free(points_list);
        return NULL;
      }

      if (i != (last_index + 1))
      {
        sprintf(error_string, "Vertex indexing is non-contiguous (%d followed %d).", i, last_index);
        ptr_slist_free(points_list);
        return NULL;
      }
      last_index = i;
      point_t* p = point_new(x, y, z);
      ptr_slist_append(points_list, p);
    }
  } while (status != EOF);

  // Translate to an array.
  *num_vertices = points_list->size;
  point_t* vertices = malloc(sizeof(point_t) * points_list->size);
  int i = 0;
  while (!ptr_slist_empty(points_list))
    vertices[i++] = *((point_t*)ptr_slist_pop(points_list, NULL));
  ptr_slist_free(points_list);
  log_detail("read_meshvoro_mesh: Read %d vertices (%d lines).", *num_vertices, last_index + 1);

  return vertices;
}

static cell_with_faces_t** read_mesh_cells(FILE* cell_file, 
                                           int* num_cells, 
                                           char* error_string)
{
  // Read the cells into a linked list.
  int status;
  ptr_slist_t* cells_list = ptr_slist_new();
  int line = 1;
  int min_cell_index = INT_MAX, max_cell_index = -INT_MAX;
  do 
  {
    // Read the cell header.
    char elem_name[10];
    int cell_index, num_faces;
    status = fscanf(cell_file, "voronoicell: %s number: %d faces: %d\n", 
                    elem_name, &cell_index, &num_faces);
    if (status == EOF) continue; // No more cells
    if (status != 3)
    {
      sprintf(error_string, "Error reading cell header (line %d of cell file).", line);
      ptr_slist_free(cells_list);
      return NULL;
    }
    min_cell_index = MIN(min_cell_index, cell_index);
    max_cell_index = MAX(max_cell_index, cell_index);
    ++line;

    // Read the index of the generator point in the vertex file.
    // (We don't actually use this information.)
    int center_vertex;
    status = fscanf(cell_file, "center:  %d\n", &center_vertex);
    if (status != 1)
    {
      sprintf(error_string, "Error reading cell center (line %d of cell file).", line);
      ptr_slist_free(cells_list);
      return NULL;
    }
    ++line;

    // Quick check.
    if (num_faces < 4)
    {
      sprintf(error_string, "Cell '%s' has fewer than 4 faces.", elem_name);
      ptr_slist_free(cells_list);
      return NULL;
    }

    // Set up a cell representation and append it to our list.
    cell_with_faces_t* cell = cell_with_faces_new(elem_name, cell_index, num_faces);
    ptr_slist_append_with_dtor(cells_list, cell, cell_list_dtor);

    // Read the faces of the cell.
    for (int f = 0; f < num_faces; ++f)
    {
      // Read the number of vertices in the face.
      int num_vertices;
      status = fscanf(cell_file, "vertices in face: %d\n", &num_vertices);
      if (status != 1)
      {
        sprintf(error_string, "Error reading number of face vertices (line %d of cell file).", line);
        ptr_slist_free(cells_list);
        return NULL;
      }
      ++line;

      // Read each of the vertices.
      int face_vertices[num_vertices];
      for (int v = 0; v < num_vertices; ++v)
      {
        status = fscanf(cell_file, "%d\n", &face_vertices[v]);
        if (status != 1)
        {
          sprintf(error_string, "Error reading vertex %d of face %d in cell '%s'\n(line %d of cell file).", v, f, elem_name, line);
          ptr_slist_free(cells_list);
          return NULL;
        }
        ++line;
      }

      // Quick check.
      if (num_vertices < 3)
      {
        sprintf(error_string, "Face %d in cell '%s' has fewer than 3 vertices.", f, elem_name);
        ptr_slist_free(cells_list);
        return NULL;
      }

      // Construct the face.
      cell->faces[f] = face_with_vertices_new(num_vertices, face_vertices);
    }

  } while (status != EOF);

  // Do some basic error checking.
  if (cells_list->size == 0)
  {
    sprintf(error_string, "No cells found in cell file!");
    ptr_slist_free(cells_list);
    return NULL;
  }
  if (min_cell_index != 0)
  {
    sprintf(error_string, "Minimum cell index is %d (must be 0).", min_cell_index);
    ptr_slist_free(cells_list);
    return NULL;
  }
  if (max_cell_index != cells_list->size-1)
  {
    sprintf(error_string, "Cell index space is not contiguous\n(max is %d, should be %d).", max_cell_index, cells_list->size-1);
    ptr_slist_free(cells_list);
    return NULL;
  }

  // Translate to an array.
  *num_cells = cells_list->size;
  cell_with_faces_t** cells = malloc(sizeof(cell_with_faces_t*) * cells_list->size);
  while (!ptr_slist_empty(cells_list))
  {
    // Place the cell into its proper location in the array.
    cell_with_faces_t* cell = ptr_slist_pop(cells_list, NULL);
    cells[cell->index] = cell;
  }
  ptr_slist_free(cells_list);
  log_detail("read_meshvoro_mesh: Read %d cells (%d lines).", *num_cells, line);

  return cells;
}

static int read_cell_neighbors(FILE* neighbor_file,
                               cell_with_faces_t** cells,
                               int num_cells,
                               char* error_string)
{
  // Now that we have a list of cells, we can fill in their neighbors 
  // with information from the neighbors file. Here we make the following
  // assumption:
  // - The number of faces in a cell equals the number of its neighbors.

  int status;
  int line = 1;
  int* cell_processed = malloc(sizeof(int) * num_cells);
  memset(cell_processed, 0, sizeof(int) * num_cells);
  do 
  {
    // Read the cell header.
    int cell_index, num_neighbors;
    status = fscanf(neighbor_file, " vorocell: %d num_neighbors: %d\n", 
                    &cell_index, &num_neighbors);
    if (status == EOF) continue; // No more cells
    if (status != 2)
    {
      sprintf(error_string, "Error reading cell neighbor header (line %d of neighbors file).", line);
      free(cell_processed);
      return -1;
    }
    if (num_neighbors != cells[cell_index]->num_faces)
    {
      sprintf(error_string, "Cell '%s' (%d) has %d neighbors and %d faces (must be equal).", 
              cells[cell_index]->name, cell_index, num_neighbors, cells[cell_index]->num_faces);
      free(cell_processed);
      return -1;
    }
    ++line;

    // Read the neighbor indices from the cells. Negative neighbor indices 
    // indicate that a cell abuts the boundary.
    for (int n = 0; n < num_neighbors; ++n)
    {
      int neighbor_id;
      status = fscanf(neighbor_file, "%d\n", &neighbor_id);
      if (status != 1)
      {
        sprintf(error_string, "Error reading cell neighbor (line %d of neighbors file).", line);
        free(cell_processed);
        return -1;
      }
      cells[cell_index]->neighbor_ids[n] = neighbor_id;
      ++line;
    }

    // We're done processing this cell.
    cell_processed[cell_index] = 1;

  } while (status != EOF);

  // Did we get neighbors for everyone?
  for (int i = 0; i < num_cells; ++i)
  {
    if (!cell_processed[i])
    {
      sprintf(error_string, "Cell '%s' (%d) was not assigned neighbors.", cells[i]->name, i);
      free(cell_processed);
      return -1;
    }
  }

  free(cell_processed);
  return 0;
}

static mesh_t* construct_mesh(cell_with_faces_t** cells, 
                              int num_cells, 
                              point_t* vertices,
                              int num_vertices,
                              char* error_string)
{
  mesh_t* mesh = NULL;

  // Count up the faces, edges, and nodes, and assign indices.
  int num_faces = 0, num_edges = 0, num_nodes = 0;

  // Nodes are vertices that appear in the faces of cells.
  int_int_unordered_map_t* node_ids = int_int_unordered_map_new();
  int_table_t* face_for_cells = int_table_new();
  int_table_t* edge_for_nodes = int_table_new();
  for (int c = 0; c < num_cells; ++c)
  {
    cell_with_faces_t* cell = cells[c];
    for (int f = 0; f < cell->num_faces; ++f)
    {
      face_with_vertices_t* face = cell->faces[f];

      // Get the neighboring cell.
      // (A face is an interface between two cells.)
      cell_with_faces_t* other_cell = NULL;
      int cell1, cell2;
      if (cell->neighbor_ids[f] >= 0)
      {
        other_cell = cells[cell->neighbor_ids[f]];
        cell1 = MIN(cell->index, other_cell->index);
        cell2 = MAX(cell->index, other_cell->index);
      }
      else
      {
        cell1 = cell->index;
        cell2 = cell->neighbor_ids[f];
      }

      if (!int_table_contains(face_for_cells, cell1, cell2))
      {
        // We count a new face.
        int_table_insert(face_for_cells, cell1, cell2, num_faces);

        // Make sure the two cells agree on the number of vertices in 
        // their shared face.
        if (other_cell != NULL)
        {
          // Find cell1 in the list of neighbor IDs for cell2.
          face_with_vertices_t* other_face = NULL;
          for (int ff = 0; ff < other_cell->num_faces; ++ff)
          {
            if (other_cell->neighbor_ids[ff] == cell->neighbor_ids[f])
            {
              other_face = other_cell->faces[ff];
              break;
            }
          }
          if (other_face == NULL)
          {
            sprintf(error_string, "Cells '%s' and '%s' are neighbors but do not have\n"
                                  "corresponding faces.", cell->name, other_cell->name);
            goto error;
          }
          if (other_face->num_vertices != face->num_vertices)
          {
            sprintf(error_string, "Neighboring cells '%s' and '%s' disagree about number of vertices in common face.\n"
                "(%d vs %d.)", cell->name, other_cell->name, face->num_vertices, 
                other_face->num_vertices);
            goto error;
          }
        }

        // Also count any edges in this face that haven't been counted yet.
        // NOTE: This assumes that the vertices in the face are listed in 
        // NOTE: traversal order.
        for (int i = 0; i < face->num_vertices; ++i)
        {
          int v1 = face->vertex_ids[i];
          int v2 = face->vertex_ids[(i + 1) % face->num_vertices];

          // Map these vertices to node indices.
          if (!int_int_unordered_map_contains(node_ids, v1))
          {
            int_int_unordered_map_insert(node_ids, v1, num_nodes);
            ++num_nodes;
          }
          if (!int_int_unordered_map_contains(node_ids, v2))
          {
            int_int_unordered_map_insert(node_ids, v2, num_nodes);
            ++num_nodes;
          }
          int n1 = *int_int_unordered_map_get(node_ids, v1);
          int n2 = *int_int_unordered_map_get(node_ids, v2);

          if (!int_table_contains(edge_for_nodes, MIN(n1, n2), MAX(n1, n2)))
          {
            // We count a new edge.
            int_table_insert(edge_for_nodes, MIN(n1, n2), MAX(n1, n2), num_edges);
            ++num_edges;
          }
        }
      }
      ++num_faces;
    }
  }

  // Create the actual mesh object.
  log_info("read_meshvoro_mesh: Creating mesh\n(%d cells, %d faces, %d edges, %d nodes)", 
           num_cells, num_faces, num_edges, num_nodes);
  mesh = mesh_new(num_cells, 0, num_faces, num_edges, num_nodes);

  // Fill in the data for the mesh elements.

  // Nodes.
  {
    int pos = 0, v, n;
    while (int_int_unordered_map_next(node_ids, &pos, &v, &n))
    {
      mesh->nodes[n].x = vertices[v].x;
      mesh->nodes[n].y = vertices[v].y;
      mesh->nodes[n].z = vertices[v].z;
    }
  }

  // Edges.
  {
    int_table_cell_pos_t pos = int_table_start(edge_for_nodes);
    int n1, n2, e;
    while (int_table_next_cell(edge_for_nodes, &pos, &n1, &n2, &e))
    {
      mesh->edges[e].node1 = &mesh->nodes[n1];
      mesh->edges[e].node2 = &mesh->nodes[n2];
    }
  }

  // Face -> cell and face -> edge connectivity.
  {
    // Loop over the faces separating cells.
    int_table_cell_pos_t pos = int_table_start(face_for_cells);
    int c1, c2, f;
    while (int_table_next_cell(face_for_cells, &pos, &c1, &c2, &f))
    {
      // Hook up the cells to the faces.
      mesh->faces[f].cell1 = &mesh->cells[c1];
      if (c2 >= 0)
        mesh->faces[f].cell2 = &mesh->cells[c2];

      // We always use cell 1 to get the nodes for the face, and thereby 
      // identify edges. Find the right face.
      face_with_vertices_t* face = NULL;
      for (int f1 = 0; f1 < cells[c1]->num_faces; ++f1)
      {
        if (cells[c1]->neighbor_ids[f1] == c2)
        {
          face = cells[c1]->faces[f1];
          break;
        }
      }

      // Now assemble edges for the face.
      for (int v = 0; v < face->num_vertices; ++v)
      {
        int v1 = face->vertex_ids[v];
        int v2 = face->vertex_ids[(v + 1) % face->num_vertices];

        // Map the vertices to mesh nodes.
        int n1 = *int_int_unordered_map_get(node_ids, v1);
        int n2 = *int_int_unordered_map_get(node_ids, v2);

        // Retrieve the edge index for these nodes.
        int e = *int_table_get(edge_for_nodes, n1, n2);
        
        mesh_attach_edge_to_face(mesh, &mesh->edges[e], &mesh->faces[f]);
      }
    }
  }

  // Cell -> face connectivity.
  for (int c1 = 0; c1 < mesh->num_cells; ++c1)
  {
    for (int f = 0; f < cells[c1]->num_faces; ++f)
    {
      // Find the face index corresponding to this cell's face.
      int c2 = cells[c1]->neighbor_ids[f];
      int face_id = *int_table_get(face_for_cells, c1, c2);
      mesh_attach_face_to_cell(mesh, &mesh->faces[face_id], &mesh->cells[c1]);
    }
  }

  // Compute the geometry of the mesh.
  mesh_compute_geometry(mesh);

  // Clean up.
  int_int_unordered_map_free(node_ids);
  int_table_free(face_for_cells);
  int_table_free(edge_for_nodes);

  return mesh;

error:
  int_int_unordered_map_free(node_ids);
  int_table_free(face_for_cells);
  int_table_free(edge_for_nodes);
  if (mesh != NULL)
    mesh_free(mesh);
  return NULL;
}

static mesh_t* mesh_from_meshvoro_files(FILE* cell_file, 
                                        FILE* vertex_file,
                                        FILE* neighbor_file)
{
  point_t* vertices = NULL;
  cell_with_faces_t** cells = NULL;
  char error_str[1024];

  // Read the mesh's vertices into memory.
  int num_vertices;
  vertices = read_mesh_vertices(vertex_file, &num_vertices, error_str);
  if (vertices == NULL) goto error;

  // Read the cell/face information.
  int num_cells;
  cells = read_mesh_cells(cell_file, &num_cells, error_str);
  if (cells == NULL) goto error;

  // Read the neighbor information for cells.
  int err = read_cell_neighbors(neighbor_file, cells, num_cells, error_str);
  if (err != 0) goto error;

  // Now construct a mesh object.
  mesh_t* mesh = construct_mesh(cells, num_cells, vertices, num_vertices, error_str);
  if (mesh == NULL) goto error;

  // Clean up.
  free(vertices);
  for (int i = 0; i < num_cells; ++i)
    destroy_cell(cells[i]);
  free(cells);

  return mesh;

error:
  if (vertices != NULL) free(vertices);
  if (cells != NULL) 
  {
    for (int i = 0; i < num_cells; ++i)
      destroy_cell(cells[i]);
    free(cells);
  }
  fclose(cell_file);
  fclose(vertex_file);
  fclose(neighbor_file);
  polymec_error(error_str);
  return NULL;
}

// read_meshvoro_mesh(file) -- This function reads a given polyhedral mesh 
// from a file and returns a mesh object.
int read_meshvoro_mesh(lua_State* lua)
{
  // Check the arguments.
  int num_args = lua_gettop(lua);
  if ((num_args != 3) || !lua_isstring(lua, 1) || 
      !lua_isstring(lua, 2) || !lua_isstring(lua, 3))
  {
    lua_pushstring(lua, "read_meshvoro_mesh: invalid arguments. Usage:\n"
                        "mesh = read_meshvoro_mesh(cell_file, vertex_file, neighbor_file)");
    lua_error(lua);
    return LUA_ERRRUN;
  }

  // Get the argument(s).
  const char* cell_filename = lua_tostring(lua, 1);
  const char* vertex_filename = lua_tostring(lua, 2);
  const char* neighbor_filename = lua_tostring(lua, 3);

  // Open the files for reading.
  FILE* cell_file = fopen(cell_filename, "r");
  if (cell_file == NULL)
    polymec_error("Could not open cell file '%s'.", cell_filename);
  FILE* vertex_file = fopen(vertex_filename, "r");
  if (vertex_file == NULL)
    polymec_error("Could not open vertex file '%s'.", vertex_filename);
  FILE* neighbor_file = fopen(neighbor_filename, "r");
  if (neighbor_file == NULL)
    polymec_error("Could not open neighbor file '%s'.", neighbor_filename);

  log_info("read_meshvoro_mesh: Reading inputs:\n"
           "  cell file: %s\n"
           "  vertex file: %s\n"
           "  neighbor file: %s", cell_filename, vertex_filename, neighbor_filename);

  // Read the mesh from the files.
  mesh_t* mesh = mesh_from_meshvoro_files(cell_file, vertex_file, neighbor_file);

  // Close 'em.
  fclose(cell_file);
  fclose(vertex_file);
  fclose(neighbor_file);

  // Push the mesh onto the stack.
  lua_pushmesh(lua, mesh);
  return 1;
}

