#include "core/unordered_set.h"
#include "core/unordered_map.h"
#include "core/slist.h"
#include "core/edit_mesh.h"
#include "core/newton.h"
#include "geometry/plane.h"
//#include "geometry/giftwrap_hull.h"
#include "geometry/create_unbounded_voronoi_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

// This function and context help us compute the projections of 
// interior nodes to the boundary.
typedef struct 
{
  // Generator points.
  point_t xg1, xg2;

  // Interior node positions.
  point_t xn1, xn2;

  // Rays connecting interior nodes to the boundary.
  vector_t ray1, ray2;

  // Plane object for constructing the boundary face.
  sp_func_t* plane;
} project_bnodes_context_t;

static void project_bnodes(void* context, double* X, double* F)
{
  project_bnodes_context_t* ctx = context;

  // Use the given parameter s1 to project our interior nodes
  // to a plane.
  double s1 = X[0];
  point_t xb1 = {.x = ctx->xn1.x + s1*ctx->ray1.x,
                 .y = ctx->xn1.y + s1*ctx->ray1.y,
                 .z = ctx->xn1.z + s1*ctx->ray1.z};

  // Construct a normal vector for the plane using xg1, xg2, and xb1.
  vector_t v1, v2, np;
  point_displacement(&ctx->xg1, &ctx->xg2, &v1);
  point_displacement(&ctx->xg1, &xb1, &v2);
  vector_cross(&v1, &v2, &np);
  ASSERT(vector_mag(&np) != 0.0);
  vector_normalize(&np);

  // Create the plane.
  if (ctx->plane == NULL)
    ctx->plane = plane_new(&np, &ctx->xg1);
  else
    plane_reset(ctx->plane, &np, &ctx->xg1);

  // Find the intersection of the second boundary node with the plane.
  double s2 = plane_intersect_with_line(ctx->plane, &ctx->xn2, &ctx->ray2);
  point_t xb2 = {.x = ctx->xn2.x + s2*ctx->ray2.x,
                 .y = ctx->xn2.y + s2*ctx->ray2.y,
                 .z = ctx->xn2.z + s2*ctx->ray2.z};

  // The function's value is the distance of xb2 from the plane.
  sp_func_eval(ctx->plane, &xb2, F);
}

mesh_t* create_bounded_voronoi_mesh(point_t* generators, int num_generators,
                                    point_t* boundary_generators, int num_boundary_generators,
                                    point_t* ghost_generators, int num_ghost_generators)
{
  ASSERT(generators != NULL);
  ASSERT(num_generators >= 1); 
  ASSERT(num_boundary_generators >= 0); 
  ASSERT((boundary_generators != NULL) || (num_boundary_generators == 0));
  ASSERT(num_ghost_generators >= 0); 
  ASSERT(num_ghost_generators >= 0); 
  ASSERT((ghost_generators != NULL) || (num_ghost_generators == 0));

  // Create an unbounded Voronoi tessellation using these generators.
  int num_non_ghost_generators = num_generators + num_boundary_generators;
  point_t non_ghost_generators[num_non_ghost_generators];
  memcpy(non_ghost_generators, generators, sizeof(point_t) * num_generators);
  memcpy(&non_ghost_generators[num_generators], boundary_generators, sizeof(point_t) * num_boundary_generators);
  mesh_t* mesh = create_unbounded_voronoi_mesh(non_ghost_generators, num_non_ghost_generators,
                                               ghost_generators, num_ghost_generators);
  ASSERT(mesh_has_tag(mesh->cell_tags, "outer_cells"));
  ASSERT(mesh_property(mesh, "outer_cell_edges") != NULL);
  ASSERT(mesh_property(mesh, "outer_rays") != NULL);

  // Fetch the relevent properties from the mesh.
  int_ptr_unordered_map_t* outer_cell_edges = mesh_property(mesh, "outer_cell_edges");
  int_ptr_unordered_map_t* outer_edge_rays = mesh_property(mesh, "outer_rays");

  // We use this map to keep track of boundary nodes we've created.
  int_int_unordered_map_t* bnode_map = int_int_unordered_map_new(); // Maps interior nodes to boundary nodes.
  int_int_unordered_map_t* generator_bnode_map = int_int_unordered_map_new(); // Maps cells to generator-point boundary nodes.

  // We use this map to keep track of boundary edges we've created.
  int_int_unordered_map_t* bedge1_map = int_int_unordered_map_new(); // Maps cells to edges connecting generator-point node to node 1.
  int_int_unordered_map_t* bedge2_map = int_int_unordered_map_new(); // Maps cells to edges connecting generator-point node to node 2.

  // A nonlinear system for projecting boundary nodes.
  project_bnodes_context_t proj_context = {.plane = NULL};
  nonlinear_system_t proj_sys = {.dim = 1, .compute_F = project_bnodes, .context = (void*)&proj_context};

  // Now traverse the boundary generators and cut them up as needed.
  for (int c = num_generators; c < num_non_ghost_generators; ++c)
  {
    // This generator should describe an outer cell. If it doesn't, we have 
    // an open boundary.
    if (!int_ptr_unordered_map_contains(outer_cell_edges, c))
    {
      polymec_error("create_bounded_voronoi_mesh: boundary generators describe\n"
                    "an open boundary at x = (%g, %g, %g)! The boundary must be closed.", 
                    non_ghost_generators[c].x, non_ghost_generators[c].y, non_ghost_generators[c].z);
    }

    // Generate or retrieve the boundary node that sits atop this boundary
    // cell's generator.
    node_t* generator_bnode;
    if (!int_int_unordered_map_contains(generator_bnode_map, c))
    {
      int gbnode_index = mesh_add_node(mesh);
      int_int_unordered_map_insert(generator_bnode_map, c, gbnode_index);
      generator_bnode = &mesh->nodes[gbnode_index];

      // Assign it the coordinates of the generator.
      generator_bnode->x = non_ghost_generators[c].x;
      generator_bnode->y = non_ghost_generators[c].y;
      generator_bnode->z = non_ghost_generators[c].z;
    }
    else
      generator_bnode = &mesh->nodes[*int_int_unordered_map_get(generator_bnode_map, c)];

    // Find the neighbors of this cell that are also boundary cells.
    cell_t* cell = &mesh->cells[c];
    int cell_index = cell - &mesh->cells[0];
    for (int f = 0; f < cell->num_faces; ++f)
    {
      face_t* face = cell->faces[f];
      cell_t* ncell = face_opp_cell(face, cell);
      int ncell_index = ncell - &mesh->cells[0];

      if (ncell_index < num_generators) continue; // Skip non-boundary cells.
      if (ncell_index < cell_index) continue; // This neighbor's already done.

      // This generator should also describe an outer cell.
      ASSERT(int_ptr_unordered_map_contains(outer_cell_edges, ncell_index));

      // Generate or retrieve the boundary node that sits atop the neighbor
      // cell's generator.
      node_t* neighbor_generator_bnode;
      if (!int_int_unordered_map_contains(generator_bnode_map, ncell_index))
      {
        int gbnode_index = mesh_add_node(mesh);
        int_int_unordered_map_insert(generator_bnode_map, ncell_index, gbnode_index);
        neighbor_generator_bnode = &mesh->nodes[gbnode_index];

        // Assign it the coordinates of the generator.
        neighbor_generator_bnode->x = non_ghost_generators[ncell_index].x;
        neighbor_generator_bnode->y = non_ghost_generators[ncell_index].y;
        neighbor_generator_bnode->z = non_ghost_generators[ncell_index].z;
      }
      else
        neighbor_generator_bnode = &mesh->nodes[*int_int_unordered_map_get(generator_bnode_map, ncell_index)];

      // In this type of boundary cell, a given cell c and its neighbor c'
      // share a face, and the boundary faces connected to this shared face
      // are coplanar. The boundary nodes (those nodes belonging to the 
      // shared face and the boundary faces) are also coplanar, and there 
      // are exactly two of them shared by c and c'. So c and c' each have 
      // a triangular boundary face whose vertices are its respective 
      // generator and these two boundary nodes.

      // Create the boundary nodes, unless they've already been created. 
      // These boundary nodes are projections of the "node1s" of the 
      // outer edges that are shared by c and c'.
      edge_t *outer_edge1 = NULL, *outer_edge2 = NULL;
      for (int e = 0; e < face->num_edges; ++e)
      {
        edge_t* edge = face->edges[e];
        if (edge->node2 == NULL) // This is an outer edge.
        {
          if (outer_edge1 == NULL)
            outer_edge1 = edge;
          else
          {
            outer_edge2 = edge;
            break;
          }
        }
      }

      // If we didn't find two outer edges, we don't need to slap
      // another face on this cell.
      if (outer_edge2 == NULL) continue;

      int node1_index = outer_edge1->node1 - &mesh->nodes[0];
      if (!int_int_unordered_map_contains(bnode_map, node1_index))
      {
        // Create the new node. NOTE: We don't compute its coordinates yet.
        int bnode_index = mesh_add_node(mesh);
        int_int_unordered_map_insert(bnode_map, node1_index, bnode_index);
        outer_edge1->node2 = &mesh->nodes[bnode_index];
      }
      int bnode1_index = *int_int_unordered_map_get(bnode_map, node1_index);
      node_t* bnode1 = &mesh->nodes[bnode1_index];
      int oedge1_index = outer_edge1 - &mesh->edges[0];
      vector_t* ray1 = *int_ptr_unordered_map_get(outer_edge_rays, oedge1_index);

      int node2_index = outer_edge2->node1 - &mesh->nodes[0];
      if (!int_int_unordered_map_contains(bnode_map, node2_index))
      {
        // Create the new node. NOTE: We don't compute its coordinates yet.
        int bnode_index = mesh_add_node(mesh);
        int_int_unordered_map_insert(bnode_map, node2_index, bnode_index);
        outer_edge2->node2 = &mesh->nodes[bnode_index];
      }
      int bnode2_index = *int_int_unordered_map_get(bnode_map, node2_index);
      node_t* bnode2 = &mesh->nodes[bnode2_index];
      int oedge2_index = outer_edge2 - &mesh->edges[0];
      vector_t* ray2 = *int_ptr_unordered_map_get(outer_edge_rays, oedge2_index);

      // Create the boundary faces for this cell and its neighbor.
      int near_face_index = mesh_add_face(mesh);
      face_t* near_face = &mesh->faces[near_face_index];
      mesh_add_face_to_cell(mesh, near_face, cell);

      int far_face_index = mesh_add_face(mesh);
      face_t* far_face = &mesh->faces[far_face_index];
      mesh_add_face_to_cell(mesh, far_face, ncell);

      // Create the edge that connects the two boundary nodes and add it 
      // to the boundary face. This edge shouldn't exist yet.
      int edge_connecting_nodes_index = mesh_add_edge(mesh);
      edge_t* edge_connecting_nodes = &mesh->edges[edge_connecting_nodes_index];
      edge_connecting_nodes->node1 = bnode1;
      edge_connecting_nodes->node2 = bnode2;
      mesh_add_edge_to_face(mesh, edge_connecting_nodes, near_face);
      mesh_add_edge_to_face(mesh, edge_connecting_nodes, far_face);

      // Create the edges that connect the generator to each boundary node.
      edge_t *near_edge1, *far_edge1;
      int* near_edge1_p = int_int_unordered_map_get(bedge1_map, c);
      if (near_edge1_p == NULL)
      {
        // We create these edges for this cell and its neighbor.
        int near_edge1_index = mesh_add_edge(mesh);
        int_int_unordered_map_insert(bedge1_map, c, near_edge1_index);
        near_edge1 = &mesh->edges[near_edge1_index];
        near_edge1->node1 = generator_bnode;
        near_edge1->node2 = bnode1;
      }
      else
      {
        int near_edge1_index = *near_edge1_p;
        near_edge1 = &mesh->edges[near_edge1_index];
      }

      int* far_edge1_p = int_int_unordered_map_get(bedge1_map, ncell_index);
      if (far_edge1_p == NULL)
      {
        // We create these edges for this cell and its neighbor.
        int far_edge1_index = mesh_add_edge(mesh);
        int_int_unordered_map_insert(bedge1_map, ncell_index, far_edge1_index);
        far_edge1 = &mesh->edges[far_edge1_index];
        far_edge1->node1 = neighbor_generator_bnode;
        far_edge1->node2 = bnode1;
      }
      else
      {
        int far_edge1_index = *far_edge1_p;
        far_edge1 = &mesh->edges[far_edge1_index];
      }
      mesh_add_edge_to_face(mesh, near_edge1, near_face);
      mesh_add_edge_to_face(mesh, far_edge1, far_face);

      edge_t *near_edge2, *far_edge2;
      int* near_edge2_p = int_int_unordered_map_get(bedge2_map, c);
      if (near_edge2_p == NULL)
      {
        // We create these edges for this cell and its neighbor.
        int near_edge2_index = mesh_add_edge(mesh);
        int_int_unordered_map_insert(bedge2_map, c, near_edge2_index);
        near_edge2 = &mesh->edges[near_edge2_index];
        near_edge2->node1 = generator_bnode;
        near_edge2->node2 = bnode2;
      }
      else
      {
        int near_edge2_index = *near_edge2_p;
        near_edge2 = &mesh->edges[near_edge2_index];
      }

      int* far_edge2_p = int_int_unordered_map_get(bedge2_map, ncell_index);
      if (far_edge2_p == NULL)
      {
        // We create these edges for this cell and its neighbor.
        int far_edge2_index = mesh_add_edge(mesh);
        int_int_unordered_map_insert(bedge2_map, ncell_index, far_edge2_index);
        far_edge2 = &mesh->edges[far_edge2_index];
        far_edge2->node1 = neighbor_generator_bnode;
        far_edge2->node2 = bnode2;
      }
      else
      {
        int far_edge2_index = *far_edge2_p;
        far_edge2 = &mesh->edges[far_edge2_index];
      }
      mesh_add_edge_to_face(mesh, near_edge2, near_face);
      mesh_add_edge_to_face(mesh, far_edge2, far_face);

      // Now that we have the right topology, we can do geometry. 
         
      // The normal vector of the plane is the plane that contains the 
      // two generators and both of the boundary nodes. The boundary nodes 
      // are the projections of their corresponding interior nodes to this 
      // plane. The equation relating the plane's normal to the coordinates 
      // of the boundary nodes forms a nonlinear equation with 
      // s1 as an unknown. We solve it here.

      // First, copy the information we need to the context.
      point_copy(&proj_context.xg1, &non_ghost_generators[c]);
      point_copy(&proj_context.xg2, &non_ghost_generators[ncell_index]);
      vector_copy(&proj_context.ray1, ray1);
      vector_copy(&proj_context.ray2, ray2);

      // Solve the nonlinear equation.
      double s1 = 0.0;
      double tolerance = 1e-6;
      int max_iters = 10, num_iters;
      newton_solve_system(&proj_sys, &s1, tolerance, max_iters, &num_iters);

      // Compute the coordinates of the boundary nodes.
      point_t xn;
      xn.x = outer_edge1->node1->x;
      xn.y = outer_edge1->node1->y;
      xn.z = outer_edge1->node1->z;
      bnode1->x = xn.x + s1*ray1->x; 
      bnode1->y = xn.y + s1*ray1->y; 
      bnode1->z = xn.z + s1*ray1->z;

      xn.x = outer_edge2->node1->x;
      xn.y = outer_edge2->node1->y;
      xn.z = outer_edge2->node1->z;
      double s2 = plane_intersect_with_line(proj_context.plane, &xn, ray2);
      bnode2->x = xn.x + s2*ray2->x; 
      bnode2->y = xn.y + s2*ray2->y; 
      bnode2->z = xn.z + s2*ray2->z;

      // Compute the areas and centers of the faces.
      vector_t v1, v2;
      node_displacement(generator_bnode, bnode1, &v1);
      node_displacement(generator_bnode, bnode2, &v2);
      near_face->area = vector_cross_mag(&v1, &v2);
      near_face->center.x = (generator_bnode->x + bnode1->x + bnode2->x) / 3.0;
      near_face->center.y = (generator_bnode->y + bnode1->y + bnode2->y) / 3.0;
      near_face->center.z = (generator_bnode->z + bnode1->z + bnode2->z) / 3.0;

      node_displacement(neighbor_generator_bnode, bnode1, &v1);
      node_displacement(neighbor_generator_bnode, bnode2, &v2);
      far_face->area = vector_cross_mag(&v1, &v2);
      far_face->center.x = (neighbor_generator_bnode->x + bnode1->x + bnode2->x) / 3.0;
      far_face->center.y = (neighbor_generator_bnode->y + bnode1->y + bnode2->y) / 3.0;
      far_face->center.z = (neighbor_generator_bnode->z + bnode1->z + bnode2->z) / 3.0;
    }
  }

  // Clean up.
  int_int_unordered_map_free(bedge1_map);
  int_int_unordered_map_free(bedge2_map);
  int_int_unordered_map_free(generator_bnode_map);
  int_int_unordered_map_free(bnode_map);

  // Before we go any further, check to see if any outer edges are 
  // poking through our boundary generators.
  // Compute the volume and center of each boundary cell.
  for (int c = num_generators; c < num_non_ghost_generators; ++c)
  {
    cell_t* cell = &mesh->cells[c];
    for (int f = 0; f < cell->num_faces; ++f)
    {
      face_t* face = cell->faces[f];
      for (int e = 0; e < face->num_edges; ++e)
      {
        edge_t* edge = face->edges[e];
        if (edge->node2 == NULL)
        {
          int edge_index = edge - &mesh->edges[0];
          polymec_error("create_bounded_voronoi_mesh: Outer edge %d does not attach to\n"
                        "a face on any boundary generator! This usually means that the boundary\n"
                        "generators do not cover the boundary.", edge_index);
        }
      }
    }
  }

  // Delete the outer_* mesh properties.
  mesh_delete_property(mesh, "outer_cell_edges");
  mesh_delete_property(mesh, "outer_rays");

  // Compute the volume and center of each boundary cell.
  for (int c = num_generators; c < num_non_ghost_generators; ++c)
  {
    cell_t* cell = &mesh->cells[c];

    // The cell center is just the average of its face centers.
    for (int f = 0; f < cell->num_faces; ++f)
    {
      face_t* face = cell->faces[f];
      cell->center.x += face->center.x;
      cell->center.y += face->center.y;
      cell->center.z += face->center.z;
    }
    cell->center.x /= cell->num_faces;
    cell->center.y /= cell->num_faces;
    cell->center.z /= cell->num_faces;

    // The volume is the sum of all tetrahedra within the cell.
    cell->volume = 0.0;
    for (int f = 0; f < cell->num_faces; ++f)
    {
      face_t* face = cell->faces[f];
      vector_t v1;
      point_displacement(&face->center, &cell->center, &v1);
      for (int e = 0; e < face->num_edges; ++e)
      {
        edge_t* edge = face->edges[e];
        ASSERT(edge->node1 != NULL);
        ASSERT(edge->node2 != NULL);

        // Construct a tetrahedron whose vertices are the cell center, 
        // the face center, and the two nodes of this edge. The volume 
        // of this tetrahedron contributes to the cell volume.
        vector_t v2, v3, v2xv3;
        point_t xn1 = {.x = edge->node1->x, .y = edge->node1->y, .z = edge->node1->z};
        point_t xn2 = {.x = edge->node2->x, .y = edge->node2->y, .z = edge->node2->z};
        point_displacement(&face->center, &xn1, &v2);
        point_displacement(&face->center, &xn2, &v3);
        vector_cross(&v2, &v3, &v2xv3);
        double tet_volume = vector_dot(&v1, &v2xv3);
        cell->volume += tet_volume;
      }
    }
  }

  return mesh;
}

#ifdef __cplusplus
}
#endif

