#include "geometry/voronoi.h"
#include "mpi.h"
#include "ctetgen.h"
#include "core/avl_tree.h"
#include "core/slist.h"
#include "core/brent.h"
#include "core/edit_mesh.h"
#include "core/faceted_surface.h"

#ifdef __cplusplus
extern "C" {
#endif

// This AVL node visitor appends the tree's data to a tag.
static void append_to_tag(int_avl_tree_node_t* node, void* p)
{
  int* tag_p = (int*)p;
  *tag_p = (int)node->attribute;
  ++tag_p;
}

mesh_t* voronoi_tessellation(point_t* points, int num_points, 
                             point_t* ghost_points, int num_ghost_points)
{
  ASSERT(points != NULL);
  ASSERT(num_points >= 2);
  ASSERT(num_ghost_points >= 0);

  // Set Tetgen's options. We desire a Voronoi mesh.
  TetGenOpts opts;
  TetGenOptsInitialize(&opts);
  opts.voroout = 1;

  // Allocate an input PLC with all the points.
  PLC* in;
  PLCCreate(&in);
  in->numberofpoints = num_points + num_ghost_points;
  PetscMalloc(3*in->numberofpoints*sizeof(double), &in->pointlist);
  memcpy(in->pointlist, (double*)points, 3*num_points);
  memcpy(&in->pointlist[3*num_points], (double*)ghost_points, 3*num_ghost_points);

  // Allocate storage for the output PLC, which will store the 
  // tetrahedra and Voronoi polyhedra.
  PLC* out;
  PLCCreate(&out);

  // Tetrahedralize.
  TetGenTetrahedralize(&opts, in, out);
  ASSERT(out->numberofvcells == (num_points + num_ghost_points));

  // Construct the Voronoi graph.
  mesh_t* mesh = mesh_new(num_points - num_ghost_points, 
                          num_ghost_points,
                          out->numberofvfacets,
                          out->numberofvedges,
                          out->numberofvpoints);
  
  // Node coordinates.
  for (int i = 0; i < mesh->num_nodes; ++i)
  {
    mesh->nodes[i].x = out->vpointlist[3*i];
    mesh->nodes[i].y = out->vpointlist[3*i+1];
    mesh->nodes[i].z = out->vpointlist[3*i+2];
  }

  // Edge <-> node connectivity.
  // NOTE: We keep track of "outer" edges and tag them accordingly.
  int_avl_tree_t* outer_edges = int_avl_tree_new();
  int num_outer_edges = 0;
  for (int i = 0; i < mesh->num_edges; ++i)
  {
    mesh->edges[i].node1 = &mesh->nodes[out->vedgelist[i].v1];
    int n2 = out->vedgelist[i].v2; // -1 if ghost
    if (n2 == -1)
    {
      int_avl_tree_insert(outer_edges, i);
      ++num_outer_edges;
      mesh->edges[i].node2 = NULL;
    }
    else
    {
      mesh->edges[i].node2 = &mesh->nodes[n2];
    }
  }
  // Tag the outer edges as such.
  if (num_outer_edges > 0)
  {
    int* outer_edge_tag = mesh_create_tag(mesh->edge_tags, "outer_edges", num_outer_edges);
    int_avl_tree_node_t* root = outer_edges->root;
    int* tag_p = outer_edge_tag;
    int_avl_tree_node_visit(root, &append_to_tag, tag_p);

    // Stick the outward rays in a "rays" property on this tag.
    double* rays = malloc(3*num_outer_edges*sizeof(double));
    mesh_tag_set_property(mesh->edge_tags, "outer_edges", "rays", rays, &free);
    for (int i = 0; i < num_outer_edges; ++i)
    {
      int j = outer_edge_tag[i];
      ASSERT(out->vedgelist[j].v2 == -1);
      rays[3*i+0] = out->vedgelist[j].vnormal[0]; 
      rays[3*i+1] = out->vedgelist[j].vnormal[1]; 
      rays[3*i+2] = out->vedgelist[j].vnormal[2]; 
    }
  }

  // Face <-> edge/cell connectivity.
  for (int i = 0; i < mesh->num_faces; ++i)
  {
    mesh->faces[i].cell1 = &mesh->cells[out->vfacetlist[i].c1];
    mesh->faces[i].cell2 = &mesh->cells[out->vfacetlist[i].c2];
    int Ne = out->vfacetlist[i].elist[0];
    mesh->faces[i].num_edges = Ne;
    for (int j = 0; j < Ne; ++j)
      mesh_add_edge_to_face(mesh, &mesh->edges[out->vfacetlist[i].elist[j+1]], &mesh->faces[i]);
  }

  // Cell <-> face connectivity.
  // Also, find and tag the "outer cells", which are the cells 
  // attached to outer edges.
  int_avl_tree_t* outer_cells = int_avl_tree_new();
  int num_outer_cells = 0;
  for (int i = 0; i < mesh->num_cells; ++i)
  {
    int Nf = out->vcelllist[i][0];
    mesh->cells[i].num_faces = Nf;
    for (int f = 0; f < Nf; ++f)
    {
      int faceid = out->vcelllist[i][f+1];
      face_t* face = &mesh->faces[faceid];
      mesh_add_face_to_cell(mesh, face, &mesh->cells[i]);
      for (int e = 0; e < face->num_edges; ++e)
      {
        int edgeid = out->vfacetlist[faceid].elist[e+1];
        if (int_avl_tree_find(outer_edges, edgeid) != NULL)
        {
          // We found an outer edge attached to this cell, which 
          // makes it an outer cell
          int_avl_tree_insert(outer_cells, i);
          ++num_outer_cells;
          break;
        }
      }
    }
  }
  // Tag the outer cells as such.
  ASSERT(num_outer_cells > 0);
  int* outer_cell_tag = mesh_create_tag(mesh->cell_tags, "outer_cells", num_outer_cells);
  int_avl_tree_node_t* root = outer_cells->root;
  int* tag_p = outer_cell_tag;
  int_avl_tree_node_visit(root, &append_to_tag, tag_p);

  // Finally, we create properties on the outer_edges and outer_cells tags 
  // that associate one with the other.
  slist_t* outer_cell_edges = slist_new(NULL);
  for (int i = 0; i < num_outer_cells; ++i)
  {
    int num_edges = 0;
    slist_node_t* pos = slist_back(outer_cell_edges);
    for (int f = 0; f < mesh->cells[i].num_faces; ++f)
    {
      int faceid = out->vcelllist[i][f+1];
      for (int e = 0; e < mesh->faces[f].num_edges; ++e)
      {
        int edgeid = out->vfacetlist[faceid].elist[e+1];
        if (int_avl_tree_find(outer_edges, edgeid) != NULL)
        {
          slist_append(outer_cell_edges, (void*)edgeid);
          ++num_edges;
        }
      }
    }
    pos = pos->next;
    slist_insert(outer_cell_edges, pos, (void*)num_edges);
  }

  // Add 'outer_edges' as a property of the outer_cells.
  int* oce = malloc(slist_size(outer_cell_edges)*sizeof(double));
  mesh_tag_set_property(mesh->edge_tags, "outer_cells", "outer_edges", outer_cell_edges, &free);
  int offset = 0;
  for (slist_node_t* n = slist_front(outer_cell_edges); n != NULL;)
  {
    // Read the number of edges for the cell.
    int num_edges = (int)n->value;
    n = n->next;
    oce[offset++] = num_edges;
    for (int e = 0; e < num_edges; ++e)
    {
      oce[offset++] = (int)n->value;
      n = n->next;
    }
  }

  // Clean up.
  slist_free(outer_cell_edges);
  int_avl_tree_free(outer_cells);
  int_avl_tree_free(outer_edges);
  PLCDestroy(&in);
  PLCDestroy(&out);

  return mesh;
}

typedef struct 
{
  sp_func_t* B;
  node_t* node;
  vector_t ray;
} boundary_int_context;

// Boundary function, parametrized to parameter s.
static double boundary_intersection(void* context, double s)
{
  boundary_int_context* C = (boundary_int_context*)context;
  point_t x = {C->node->x + C->ray.x*s, 
               C->node->y + C->ray.y*s, 
               C->node->z + C->ray.z*s};
  double val;
  sp_func_eval(C->B, &x, &val);
  return val;
}

faceted_surface_t* voronoi_intersect_with_boundary(mesh_t* mesh, sp_func_t* boundary)
{
  // We use the outer edges to find the intersection.
  int num_outer_edges;
  int* outer_edges = mesh_tag(mesh->edge_tags, "outer_edges", &num_outer_edges);
  ASSERT(outer_edges != NULL);

  // The number of nodes in the faceted surface is equal to the number of 
  // outer edges.
  int num_surf_nodes = num_outer_edges;
  node_t surf_nodes[num_outer_edges];

  // Get the rays and use them to parametrize the intersection of the 
  // boundary.
  double* rays = mesh_tag_property(mesh->edge_tags, "outer_edges", "rays");
  ASSERT(rays != NULL);

  // We compute their positions by finding those points for which
  // our boundary function is zero.
  boundary_int_context ctx;
  ctx.B = boundary;

  for (int i = 0; i < num_outer_edges; ++i)
  {
    // Set up the context to do a nonlinear solve.
    edge_t* edge = &mesh->edges[outer_edges[i]];
    ctx.node = edge->node1;
    ctx.ray.x = rays[3*i];
    ctx.ray.y = rays[3*i+1];
    ctx.ray.z = rays[3*i+2];

    // Use Brent's method to find s within the given tolerance.
    // tolerance.
    double tol = 1e-5;
    double s, error; 
    s = brent_solve(&boundary_intersection, &ctx, 0.0, FLT_MAX, tol, 10, &error);

    // Calculate the boundary node position.
    surf_nodes[i].x = ctx.node->x + s*ctx.ray.x;
    surf_nodes[i].y = ctx.node->y + s*ctx.ray.y;
    surf_nodes[i].z = ctx.node->z + s*ctx.ray.z;
  }

  // Now we generate the surface faces and their bounding edges.
  // FIXME
  int num_surf_faces = 0, num_surf_edges = 0;

  // Create the surface.
  faceted_surface_t* surface = faceted_surface_new_with_arena(mesh->arena, num_surf_faces, num_surf_edges, num_surf_nodes);
  memcpy(surface->nodes, surf_nodes, num_surf_nodes*sizeof(node_t));

  return surface;
}

void voronoi_prune(mesh_t* mesh)
{
  // Go over all of the outer cells and remove them from the mesh.
  int num_outer_cells;
  int* outer_cells = mesh_tag(mesh->cell_tags, "outer_cells", &num_outer_cells);
  for (int i = 0; i < num_outer_cells; ++i)
    mesh_delete_cell(mesh, outer_cells[i]);

  // Now do the same for the outer edges.
  int num_outer_edges;
  int* outer_edges = mesh_tag(mesh->edge_tags, "outer_edges", &num_outer_edges);
  for (int i = 0; i < num_outer_edges; ++i)
    mesh_delete_edge(mesh, outer_edges[i]);
}

mesh_t* voronoi_tessellation_within_surface(point_t* points, int num_points,
                                            point_t* ghost_points, int num_ghost_points,
                                            faceted_surface_t* surface)
{
  // FIXME
  return NULL;
}

#ifdef __cplusplus
}
#endif

