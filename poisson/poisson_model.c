#include <string.h>
#include <stdlib.h>
#include "petscksp.h"
#include "petscmat.h"
#include "petscvec.h"
#include "core/unordered_map.h"
#include "core/least_squares.h"
#include "core/linear_algebra.h"
#include "core/constant_st_func.h"
#include "geometry/cubic_lattice.h"
#include "geometry/create_cubic_lattice_mesh.h"
#include "geometry/create_cvt.h"
#include "geometry/plane.h"
#include "geometry/cylinder.h"
#include "geometry/sphere.h"
#include "geometry/intersection.h"
#include "io/silo_io.h"
#include "io/vtk_plot_io.h"
#include "poisson/poisson_model.h"
#include "poisson/laplacian_op.h"

#ifdef __cplusplus
extern "C" {
#endif

// Boundary condition structure.
// This represents a generic boundary condition: 
// alpha * phi + beta * dphi/dn = F
typedef struct
{
  double alpha, beta;
  st_func_t* F;
} poisson_bc_t;

static poisson_bc_t* create_bc(double alpha, double beta, st_func_t* F)
{
  ASSERT(F != NULL);
  poisson_bc_t* bc = malloc(sizeof(poisson_bc_t));
  bc->alpha = alpha;
  bc->beta = beta;
  bc->F = F;
  return bc;
}

static void free_bc(void* bc)
{
  poisson_bc_t* pbc = (poisson_bc_t*)bc;
  pbc->F = NULL;
  free(pbc);
}

// Metadata for boundary cells (those cells that touch boundary faces).
typedef struct
{
  int num_neighbor_cells;
  int* neighbor_cells;

  int num_boundary_faces;
  int* boundary_faces;
  poisson_bc_t** bc_for_face;
} poisson_boundary_cell_t;

static poisson_boundary_cell_t* create_boundary_cell()
{
  poisson_boundary_cell_t* bcell = malloc(sizeof(poisson_boundary_cell_t));
  bcell->neighbor_cells = NULL;
  bcell->num_neighbor_cells = 0;
  bcell->boundary_faces = NULL;
  bcell->num_boundary_faces = 0;
  bcell->bc_for_face = NULL;
  return bcell;
}

static void free_boundary_cell(poisson_boundary_cell_t* cell)
{
  if (cell->neighbor_cells != NULL)
    free(cell->neighbor_cells);
  if (cell->boundary_faces != NULL)
    free(cell->boundary_faces);
  if (cell->bc_for_face != NULL)
    free(cell->bc_for_face);
  free(cell);
}

// Poisson model context structure.
typedef struct 
{
  mesh_t* mesh;             // Mesh.
  st_func_t* rhs;           // Right-hand side function.
  double* phi;              // Solution array.
  lin_op_t* L;              // Laplacian operator.

  str_ptr_unordered_map_t* bcs; // Boundary conditions.
  poly_ls_shape_t* shape;       // Least-squares shape functions.

  // Information for boundary cells.
  int_ptr_unordered_map_t*  boundary_cells;

  // This flag is true if the source function or one of the BCs is 
  // time-dependent, and false otherwise.
  bool is_time_dependent;

  KSP solver;               // Linear solver.
  Mat A;                    // Laplacian matrix.
  Vec x;                    // Solution vector.
  Vec b;                    // RHS vector. 

  bool initialized;         // Initialized flag.
  MPI_Comm comm;            // MPI communicator.

} poisson_t;

// A proper constructor.
static model_t* create_poisson(mesh_t* mesh, st_func_t* rhs, str_ptr_unordered_map_t* bcs, options_t* options)
{
  model_t* poisson = poisson_model_new(options);
  poisson_t* p = (poisson_t*)model_context(poisson);
  p->mesh = mesh;
  p->rhs = rhs;
  if (p->bcs != NULL)
    str_ptr_unordered_map_free(p->bcs);
  p->bcs = bcs;
  p->L = laplacian_op_new(p->mesh);

  // Determine whether this model is time-dependent.
  p->is_time_dependent = !st_func_is_constant(p->rhs);
  int pos = 0;
  char* tag;
  poisson_bc_t* bc;
  while (str_ptr_unordered_map_next(bcs, &pos, &tag, (void**)&bc))
  {
    // If any BC is time-dependent, the whole problem is.
    if (!st_func_is_constant(bc->F))
      p->is_time_dependent = true;
  }

  return poisson;
}

//------------------------------------------------------------------------
//                            Benchmarks
//------------------------------------------------------------------------

// Mesh generation for benchmark problems.
static mesh_t* create_cube_mesh(int dim, int N, bbox_t* bbox)
{
  // Get the characteristic resolution.
  int N3[3] = {N, 1, 1};
  for (int d = 0; d < dim; ++d)
    N3[d] = N;

  // Create the mesh.
  mesh_t* mesh = create_cubic_lattice_mesh_with_bbox(N3[0], N3[1], N3[2], bbox);

  // Tag the boundaries of the mesh.
  cubic_lattice_t* lattice = cubic_lattice_new(N3[0], N3[1], N3[2]);
  int* x1tag = mesh_create_tag(mesh->face_tags, "-x", N3[1]*N3[2]);
  int* x2tag = mesh_create_tag(mesh->face_tags, "+x", N3[1]*N3[2]);
  for (int j = 0; j < N3[1]; ++j)
  {
    for (int k = 0; k < N3[2]; ++k)
    {
      x1tag[N3[2]*j + k] = cubic_lattice_x_face(lattice, 0, j, k);
      x2tag[N3[2]*j + k] = cubic_lattice_x_face(lattice, N3[0], j, k);
    }
  }

  int* y1tag = mesh_create_tag(mesh->face_tags, "-y", N3[0]*N3[2]);
  int* y2tag = mesh_create_tag(mesh->face_tags, "+y", N3[0]*N3[2]);
  for (int i = 0; i < N3[0]; ++i)
  {
    for (int k = 0; k < N3[2]; ++k)
    {
      y1tag[N3[2]*i + k] = cubic_lattice_y_face(lattice, i, 0, k);
      y2tag[N3[2]*i + k] = cubic_lattice_y_face(lattice, i, N3[1], k);
    }
  }

  int* z1tag = mesh_create_tag(mesh->face_tags, "-z", N3[0]*N3[1]);
  int* z2tag = mesh_create_tag(mesh->face_tags, "+z", N3[0]*N3[1]);
  for (int i = 0; i < N3[0]; ++i)
  {
    for (int j = 0; j < N3[1]; ++j)
    {
      z1tag[N3[1]*i + j] = cubic_lattice_z_face(lattice, i, j, 0);
      z2tag[N3[1]*i + j] = cubic_lattice_z_face(lattice, i, j, N3[2]);
    }
  }
  lattice = NULL;

  return mesh;
}

static void run_analytic_problem(mesh_t* mesh, st_func_t* rhs, str_ptr_unordered_map_t* bcs, options_t* options, double t1, double t2, st_func_t* solution, double* lp_norms)
{
  // Create the model.
  model_t* model = create_poisson(mesh, rhs, bcs, options);

  // Run the thing.
  model_run(model, t1, t2);

  // Calculate the Lp norm of the error and write it to Lp_norms.
  poisson_t* pm = model_context(model);
  double Linf = 0.0, L1 = 0.0, L2 = 0.0;
  for (int c = 0; c < pm->mesh->num_cells; ++c)
  {
    double phi_sol;
    st_func_eval(solution, &pm->mesh->cells[c].center, t2, &phi_sol);
    double V = pm->mesh->cells[c].volume;
    double err = fabs(pm->phi[c] - phi_sol);
//printf("i = %d, phi = %g, phi_s = %g, err = %g\n", c, pm->phi[c], phi_sol, err);
    Linf = (Linf < err) ? err : Linf;
    L1 += err*V;
    L2 += err*err*V*V;
  }
  L2 = sqrt(L2);
  lp_norms[0] = Linf;
  lp_norms[1] = L1;
  lp_norms[2] = L2;
//  norm_t* lp_norm = fv2_lp_norm_new(pm->mesh);
//  for (int p = 0; p <= 2; ++p)
//    lp_norms[p] = fv2_lp_norm_compute_error_from_solution(p, 
  // FIXME

  // Clean up.
//  lp_norm = NULL;
  model_free(model);
}

static void laplace_1d_solution(void* context, point_t* x, double t, double* phi)
{
  phi[0] = 1.0 + 2.0*x->x;
}

static void laplace_1d_solution_grad(void* context, point_t* x, double t, double* grad_phi)
{
  grad_phi[0] = 2.0;
  grad_phi[1] = 0.0;
  grad_phi[2] = 0.0;
}

static void poisson_run_laplace_1d(options_t* options, int dim)
{
  // RHS function is zero for Laplace's equation.
  double z = 0.0;
  st_func_t* zero = constant_st_func_new(1, &z);

  // Analytic solution and gradient.
  st_func_t* sol = st_func_from_func("laplace_1d_sol", laplace_1d_solution,
                                     ST_INHOMOGENEOUS, ST_CONSTANT, 1);
  st_func_t* sol_grad = st_func_from_func("laplace_1d_sol_grad", laplace_1d_solution_grad,
                                          ST_INHOMOGENEOUS, ST_CONSTANT, 3);

  // Boundary conditions: Dirichlet on -x/+x, zero flux on the others.
  str_ptr_unordered_map_t* bcs = str_ptr_unordered_map_new();
  str_ptr_unordered_map_insert(bcs, "-x", create_bc(1.0, 0.0, sol));
  str_ptr_unordered_map_insert(bcs, "+x", create_bc(1.0, 0.0, sol));
  str_ptr_unordered_map_insert(bcs, "-y", create_bc(0.0, 1.0, zero));
  str_ptr_unordered_map_insert(bcs, "+y", create_bc(0.0, 1.0, zero));
  str_ptr_unordered_map_insert(bcs, "-z", create_bc(0.0, 1.0, zero));
  str_ptr_unordered_map_insert(bcs, "+z", create_bc(0.0, 1.0, zero));

  // Run time.
  double t = 0.0;

  // Base resolution, number of runs.
  int N0;
  switch(dim)
  {
    case 1: 
      N0 = 32;
      break;
    case 2:
      N0 = 16;
      break;
    case 3:
      N0 = 8;
      break;
  }
  int num_runs = 2;

  // Do a convergence study.
  double Lp_norms[num_runs][3];
  for (int iter = 0; iter < num_runs; ++iter)
  {
    int N = N0 * pow(2, iter);
    double dx = 1.0/N;
    bbox_t bbox = {.x1 = 0.0, .x2 = 1.0, .y1 = 0.0, .y2 = 1.0, .z1 = 0.0, .z2 = 1.0};
    if (dim == 1)
      bbox.y2 = bbox.z2 = dx;
    if (dim == 2)
      bbox.z2 = dx;
    mesh_t* mesh = create_cube_mesh(dim, N, &bbox);
    str_ptr_unordered_map_t* bcs_copy = str_ptr_unordered_map_copy(bcs);
    run_analytic_problem(mesh, zero, bcs_copy, options, t, t, sol, Lp_norms[iter]);

    // If we run in 1D or 2D, we need to adjust the norms.
    if (dim == 1)
    {
      Lp_norms[iter][1] /= dx*dx;
      Lp_norms[iter][2] /= dx*dx;
    }
    else if (dim == 2)
    {
      Lp_norms[iter][1] /= dx;
      Lp_norms[iter][2] /= dx;
    }
    log_info("iteration %d (Nx = %d): L1 = %g, L2 = %g, Linf = %g", iter, N, Lp_norms[iter][1], Lp_norms[iter][2], Lp_norms[iter][0]);
  }

  // Clean up.
  int pos = 0;
  char* key;
  void* value;
  while (str_ptr_unordered_map_next(bcs, &pos, &key, &value))
    free_bc(value);
  str_ptr_unordered_map_free(bcs);
  zero = NULL;
  sol = NULL;
  sol_grad = NULL;
}

static void run_laplace_1d(options_t* options)
{
  poisson_run_laplace_1d(options, 1);
}

static void run_laplace_1d_2(options_t* options)
{
  poisson_run_laplace_1d(options, 2);
}

static void run_laplace_1d_3(options_t* options)
{
  poisson_run_laplace_1d(options, 3);
}

#if 0
static void poisson_run_laplace_sov(int variant, options_t* options)
{
  // Dimension.
  int dim;
  // FIXME

  // RHS function is zero for Laplace's equation.
  double zero = 0.0;
  st_func_t* rhs = constant_st_func_new(1, &zero);

  // Analytic solution and/or gradient.
  st_func_t* sol;
  // FIXME = create_paraboloid_solution(variant);

  // Boundary conditions.
  str_ptr_unordered_map_t* bcs = str_ptr_unordered_map_new();
  if ((variant % 3) == 0)
    str_ptr_unordered_map_insert(bcs, "boundary", create_bc(1.0, 0.0, NULL));
  else if ((variant % 3) == 1)
    str_ptr_unordered_map_insert(bcs, "boundary", create_bc(0.0, 1.0, NULL));
  else // if ((variant % 3) == 2)
    str_ptr_unordered_map_insert(bcs, "boundary", create_bc(1.0, 1.0, NULL));

  // Start/end times.
  double t1, t2;
  // FIXME

  // Base resolution.
  int N0;
  // FIXME

  // Number of refinements.
  int num_refinements;
  // FIXME

  // Do a convergence study.
  double Lpnorms[num_refinements][3];
  for (int iter = 0; iter < num_refinements; ++iter)
  {
    int N = pow(N0, iter+1);
    bbox_t bbox;
    mesh_t* mesh = create_cube_mesh(dim, N, &bbox);
    run_analytic_problem(mesh, rhs, bcs, options, t1, t2, sol, Lpnorms[iter]);
  }
}
#endif

static void paraboloid_solution(void* context, point_t* x, double t, double* phi)
{
  static point_t origin = {.x = 0.0, .y = 0.0, .z = 0.0 };
  double r2 = point_square_distance(x, &origin);
  phi[0] = 1.0 + 2.0*r2;
}

static void poisson_run_paraboloid(options_t* options, int dim)
{
  ASSERT((dim == 2) || (dim == 3));

  // RHS function.
  double four = 4.0;
  st_func_t* rhs = constant_st_func_new(1, &four);

  // Analytic solution.
  st_func_t* sol = st_func_from_func("paraboloid", paraboloid_solution,
                                     ST_INHOMOGENEOUS, ST_CONSTANT, 1);

  // Set up a Dirichlet boundary condition along each of the outside faces.
  str_ptr_unordered_map_t* bcs = str_ptr_unordered_map_new();
  str_ptr_unordered_map_insert(bcs, "+x", create_bc(1.0, 0.0, sol));
  str_ptr_unordered_map_insert(bcs, "-x", create_bc(1.0, 0.0, sol));
  str_ptr_unordered_map_insert(bcs, "+y", create_bc(1.0, 0.0, sol));
  str_ptr_unordered_map_insert(bcs, "-y", create_bc(1.0, 0.0, sol));
  
  // Set up a homogeneous Neumann boundary condition on +/- z.
  double z = 0.0;
  st_func_t* zero = constant_st_func_new(1, &z);
  str_ptr_unordered_map_insert(bcs, "+z", create_bc(0.0, 1.0, zero));
  str_ptr_unordered_map_insert(bcs, "-z", create_bc(0.0, 1.0, zero));

  // Start/end time.
  double t = 0.0;

  // Base resolution and number of runs.
  int N0;
  int num_runs;
  switch (dim)
  {
    case 2:
      N0 = 16;
      num_runs = 3;
      break;
    case 3:
      N0 = 8;
      num_runs = 2;
      break;
  }
  num_runs = 2;
  
  // Do a convergence study.
  double Lpnorms[num_runs][3];
  for (int iter = 0; iter < num_runs; ++iter)
  {
    int N = pow(N0, iter+1);
    bbox_t bbox = {.x1 = -0.5, .x2 = 0.5, .y1 = -0.5, .y2 = 0.5, .z1 = 0.0, .z2 = 1.0};
    if (dim == 2)
      bbox.z2 = 1.0/N;
    mesh_t* mesh = create_cube_mesh(dim, N, &bbox);
    str_ptr_unordered_map_t* bcs_copy = str_ptr_unordered_map_copy(bcs);
    run_analytic_problem(mesh, rhs, bcs_copy, options, t, t, sol, Lpnorms[iter]);
  }

  // Clean up.
  int pos = 0;
  char* key;
  void* value;
  while (str_ptr_unordered_map_next(bcs, &pos, &key, &value))
    free_bc(value);
  str_ptr_unordered_map_free(bcs);
  zero = NULL;
  sol = NULL;
}

static void run_paraboloid(options_t* options)
{
  poisson_run_paraboloid(options, 2);
}

//------------------------------------------------------------------------
//                        Model implementation
//------------------------------------------------------------------------

// Turn the map of boundary conditions into a map of information for each boundary cell.
static void initialize_boundary_cells(str_ptr_unordered_map_t* bcs, mesh_t* mesh, int_ptr_unordered_map_t* boundary_cells)
{
  int pos = 0;
  char* tag;
  poisson_bc_t* bc;
  while (str_ptr_unordered_map_next(bcs, &pos, &tag, (void**)&bc))
  {
    // Retrieve the tag for this boundary condition.
    ASSERT(mesh_has_tag(mesh->face_tags, tag));
    int num_faces;
    int* faces = mesh_tag(mesh->face_tags, tag, &num_faces);

    // Now create an entry for each boundary cell and count boundary
    // faces and neighbors.
    for (int f = 0; f < num_faces; ++f)
    {
      face_t* face = &mesh->faces[faces[f]];
      ASSERT(face->cell2 == NULL); // ... true for now.

      // Get the cell for this boundary face. This is the boundary cell.
      cell_t* cell = face->cell1;
      int bcell = cell - &mesh->cells[0];

      // Have we created an entry for this one yet? If not, do so.
      poisson_boundary_cell_t* boundary_cell;
      if (!int_ptr_unordered_map_contains(boundary_cells, bcell))
      {
        boundary_cell = create_boundary_cell();
        int_ptr_unordered_map_insert(boundary_cells, bcell, boundary_cell);

        // Gather the interior faces for the cell.
        for (int ff = 0; ff < cell->num_faces; ++ff)
        {
          if (face_opp_cell(cell->faces[ff], cell) != NULL)
            boundary_cell->num_neighbor_cells++;
        }
        boundary_cell->neighbor_cells = malloc(sizeof(int)*boundary_cell->num_neighbor_cells);
        int nc = 0;
        for (int ff = 0; ff < cell->num_faces; ++ff)
        {
          cell_t* opp_cell = face_opp_cell(cell->faces[ff], cell);
          if (opp_cell != NULL)
          {
            int opp = opp_cell - &mesh->cells[0];
            boundary_cell->neighbor_cells[nc++] = opp;
          }
        }
      }
      else
      {
        // Just retrieve the existing boundary cell.
        boundary_cell = *int_ptr_unordered_map_get(boundary_cells, bcell);
      }

      // Now increment the boundary face count for this cell.
      boundary_cell->num_boundary_faces++;
    }
  }

  // Allocate storage for the boundary faces and BCs within the cells.
  {
    int pos = 0;
    int bcell_index;
    poisson_boundary_cell_t* bcell;
    while (int_ptr_unordered_map_next(boundary_cells, &pos, &bcell_index, (void**)&bcell))
    {
      bcell->boundary_faces = malloc(sizeof(int)*bcell->num_boundary_faces);
      for (int f = 0; f < bcell->num_boundary_faces; ++f)
        bcell->boundary_faces[f] = -1;
      bcell->bc_for_face = malloc(sizeof(poisson_bc_t*)*bcell->num_boundary_faces);
    }
  }

  // Now go back through and set the boundary faces and boundary 
  // conditions for each cell.
  pos = 0;
  while (str_ptr_unordered_map_next(bcs, &pos, &tag, (void**)&bc))
  {
    // Retrieve the tag for this boundary condition.
    ASSERT(mesh_has_tag(mesh->face_tags, tag));
    int num_faces;
    int* faces = mesh_tag(mesh->face_tags, tag, &num_faces);

    // Now create an entry for each boundary cell and count boundary
    // faces and neighbors.
    for (int f = 0; f < num_faces; ++f)
    {
      face_t* face = &mesh->faces[faces[f]];
      cell_t* cell = face->cell1;
      int bcell = cell - &mesh->cells[0];

      poisson_boundary_cell_t* boundary_cell = *int_ptr_unordered_map_get(boundary_cells, bcell);
      ASSERT(boundary_cell != NULL);

      int i = 0;
      while (boundary_cell->boundary_faces[i] != -1) ++i;
      boundary_cell->boundary_faces[i] = faces[f];
      boundary_cell->bc_for_face[i] = bc;
    }
  }
}

// Apply boundary conditions to a set of boundary cells.
static void apply_bcs(int_ptr_unordered_map_t* boundary_cells,
                      mesh_t* mesh,
                      poly_ls_shape_t* shape,
                      double t,
                      Mat A,
                      Vec b)
{
  // Go over the boundary cells, enforcing boundary conditions on each 
  // face.
  MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY);
  VecAssemblyBegin(b);
  int pos = 0, bcell;
  poisson_boundary_cell_t* cell_info;
  while (int_ptr_unordered_map_next(boundary_cells, &pos, &bcell, (void**)(&cell_info)))
  {
    cell_t* cell = &mesh->cells[bcell];

    // Construct a polynomial least-squares fit for the cell and its 
    // neighbors (about its center), plus any boundary faces. That is,
    // construct a fit that satisfies the boundary conditions!

    // Number of points = number of neighbors + self + boundary faces
    int num_ghosts = cell_info->num_boundary_faces;
    int num_neighbors = cell_info->num_neighbor_cells;
    int num_points = num_neighbors + 1 + num_ghosts;
//    printf("num_ghosts = %d, num_points = %d\n", num_ghosts, num_points);
    point_t points[num_points];
    points[0].x = mesh->cells[bcell].center.x;
    points[0].y = mesh->cells[bcell].center.y;
    points[0].z = mesh->cells[bcell].center.z;
//printf("ipoint = %g %g %g\n", points[0].x, points[0].y, points[0].z);
    for (int n = 0; n < num_neighbors; ++n)
    {
      int neighbor = cell_info->neighbor_cells[n];
      points[n+1].x = mesh->cells[neighbor].center.x;
      points[n+1].y = mesh->cells[neighbor].center.y;
      points[n+1].z = mesh->cells[neighbor].center.z;
    }
    int ghost_point_indices[num_ghosts];
    point_t constraint_points[num_ghosts];
    for (int n = 0; n < num_ghosts; ++n)
    {
      int bface = cell_info->boundary_faces[n];
      face_t* face = &mesh->faces[bface];
      int offset = 1 + num_neighbors;
      ghost_point_indices[n] = n+offset;
      points[n+offset].x = 2.0*face->center.x - cell->center.x;
      points[n+offset].y = 2.0*face->center.y - cell->center.y;
      points[n+offset].z = 2.0*face->center.z - cell->center.z;
//printf("gpoint = %g %g %g\n", points[n+offset].x, points[n+offset].y, points[n+offset].z);
      constraint_points[n].x = face->center.x;
      constraint_points[n].y = face->center.y;
      constraint_points[n].z = face->center.z;
    }
    poly_ls_shape_set_domain(shape, &cell->center, points, num_points);

    // Now traverse the boundary faces of this cell and enforce the 
    // appropriate boundary condition on each. To do this, we compute 
    // the elements of an affine linear transformation that allows us 
    // to compute boundary values of the solution in terms of the 
    // interior values.
    vector_t face_normals[num_ghosts];
    double aff_matrix[num_ghosts*num_points], aff_vector[num_ghosts];
    {
      double a[num_ghosts], b[num_ghosts], c[num_ghosts], d[num_ghosts], e[num_ghosts];
      for (int f = 0; f < num_ghosts; ++f)
      {
        // Retrieve the boundary condition for the face.
        poisson_bc_t* bc = cell_info->bc_for_face[f];

        // Compute the face normal and the corresponding coefficients,
        // enforcing alpha * phi + beta * dphi/dn = F on the face.
        face_t* face = &mesh->faces[cell_info->boundary_faces[f]];
        vector_t* n = &face_normals[f];
        n->x = face->center.x - cell->center.x;
        n->y = face->center.y - cell->center.y;
        n->z = face->center.z - cell->center.z;
        vector_normalize(n);
        a[f] = bc->alpha;
        b[f] = bc->beta*n->x, c[f] = bc->beta*n->y, d[f] = bc->beta*n->z;

        // Compute F at the face center.
        st_func_eval(bc->F, &face->center, t, &e[f]);
      }

      // Now that we've gathered information about all the boundary
      // conditions, compute the affine transformation that maps the 
      // (unconstrained) values of the solution to the constrained values. 
      poly_ls_shape_compute_ghost_transform(shape, 
          ghost_point_indices, num_ghosts, constraint_points, a, b, c, d, e, aff_matrix, aff_vector);
//printf("%d: A_aff (num_ghosts = %d, np = %d) = ", bcell, num_ghosts, num_points);
//matrix_fprintf(aff_matrix, num_ghosts, num_points, stdout);
//printf("\n%d: b_aff = ", bcell);
//vector_fprintf(aff_vector, num_ghosts, stdout);
//printf("\n");
    }

    // Compute the flux through each boundary face and alter the 
    // linear system accordingly.
    int ij[num_neighbors+1];
    double N[num_points], Aij[num_neighbors+1];
    vector_t grad_N[num_points]; 
    for (int f = 0; f < num_ghosts; ++f)
    {
      // Compute the shape function values and gradients at the face center.
      face_t* face = &mesh->faces[cell_info->boundary_faces[f]];
      poly_ls_shape_compute_gradients(shape, &face->center, N, grad_N);
//printf("N = ");
//for (int i = 0; i < num_points; ++i)
//printf("%g ", N[i]);
//printf("\n");
//printf("grad N = ");
//for (int i = 0; i < num_points; ++i)
//printf("%g %g %g  ", grad_N[i].x, grad_N[i].y, grad_N[i].z);
//printf("\n");

      // Add the dphi/dn terms for face f to the matrix.
      vector_t* n = &face_normals[f];
      double bi = 0.0;

      // Diagonal term.
      ij[0] = bcell;
      // Compute the contribution to the flux from this cell.
//printf("For face %d (n = %g %g %g, x = %g %g %g):\n", f, n->x, n->y, n->z, face->center.x, face->center.y, face->center.z);
      Aij[0] = vector_dot(n, &grad_N[0]) * face->area; 
//printf("A[%d,%d] += %g * %g -> %g (%g)\n", bcell, ij[0], vector_dot(n, &grad_N[0]), face->area, Aij[0], N[0]);

      // Now compute the flux contribution from ghost points.
      for (int g = 0; g < num_ghosts; ++g)
      {
        double dNdn = vector_dot(n, &grad_N[num_neighbors+1+g]);
        Aij[0] += aff_matrix[num_ghosts*0+g] * dNdn * face->area;
//printf("A[%d,%d] += %g * %g * %g -> %g (%g)\n", bcell, ij[0], aff_matrix[g], dNdn, face->area, Aij[0], N[num_neighbors+1+g]);
        bi += -aff_vector[g] * dNdn * face->area;
      }


      for (int j = 0; j < num_neighbors; ++j)
      {
        ij[j+1] = cell_info->neighbor_cells[j];

        // Self contribution.
        Aij[j+1] = vector_dot(n, &grad_N[j+1]) * face->area;

        // Ghost contributions.
        for (int g = 0; g < num_ghosts; ++g)
        {
          double dNdn = vector_dot(n, &grad_N[num_neighbors+1+g]);
          Aij[j+1] += aff_matrix[num_ghosts*(j+1)+g] * dNdn * face->area;
//printf("A[%d,%d] += %g * %g * %g = %g (%g)\n", bcell, ij[j+1], aff_matrix[num_ghosts*(j+1)+g],vector_dot(n, &grad_N[j+1]), face->area, Aij[j+1], N[j+1]);
          // NOTE: we don't double count the affine term contribution here.
//          bi += -aff_vector[g] * dNdn * face->area;
        }
//printf("b[%d] += %g\n", bcell, bi);
      }

      MatSetValues(A, 1, &bcell, num_neighbors+1, ij, Aij, ADD_VALUES);
      VecSetValues(b, 1, &bcell, &bi, ADD_VALUES);
    }
  }
  MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY);
  VecAssemblyEnd(b);
}

static void set_up_linear_system(mesh_t* mesh, lin_op_t* L, st_func_t* rhs, double t, Mat A, Vec b)
{
  PetscInt nnz[mesh->num_cells];
  for (int i = 0; i < mesh->num_cells; ++i)
    nnz[i] = lin_op_stencil_size(L, i);

  // Set matrix entries.
  MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY);
  for (int i = 0; i < mesh->num_cells; ++i)
  {
    int indices[nnz[i]];
    double values[nnz[i]];
    lin_op_compute_stencil(L, i, indices, values);
    for (int j = 0; j < nnz[i]; ++j)
      indices[j] += i;
    MatSetValues(A, 1, &i, nnz[i], indices, values, INSERT_VALUES);
  }
  MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY);

  // Compute the RHS vector.
  double values[mesh->num_cells];
  int indices[mesh->num_cells];
  VecAssemblyBegin(b);
  for (int c = 0; c < mesh->num_cells; ++c)
  {
    indices[c] = c;
    point_t xc = {.x = 0.0, .y = 0.0, .z = 0.0};
    st_func_eval(rhs, &xc, t, &values[c]);
  }
  VecSetValues(b, mesh->num_cells, indices, values, INSERT_VALUES);
  VecAssemblyEnd(b);
}

static void poisson_advance(void* context, double t, double dt)
{
  poisson_t* p = (poisson_t*)context;

  // If we're time-dependent, recompute the linear system here.
  if (!p->is_time_dependent)
  {
    set_up_linear_system(p->mesh, p->L, p->rhs, t+dt, p->A, p->b);
    apply_bcs(p->boundary_cells, p->mesh, p->shape, t+dt, p->A, p->b);
  }
//  MatView(p->A, PETSC_VIEWER_STDOUT_SELF);
//  VecView(p->b, PETSC_VIEWER_STDOUT_SELF);

  // Set up the linear solver.
  KSPSetOperators(p->solver, p->A, p->A, SAME_NONZERO_PATTERN);

  // Solve the linear system.
  KSPSolve(p->solver, p->b, p->x);

  // Copy the values from x to our solution array.
//  VecView(p->x, PETSC_VIEWER_STDOUT_SELF);
  double* x;
  VecGetArray(p->x, &x);
  memcpy(p->phi, x, sizeof(double)*p->mesh->num_cells);
  VecRestoreArray(p->x, &x);
}

static void poisson_init(void* context, double t)
{
  poisson_t* p = (poisson_t*)context;

  // If the model has been previously initialized, clean everything out.
  if (p->initialized)
  {
    KSPDestroy(&p->solver);
    MatDestroy(&p->A);
    VecDestroy(&p->x);
    VecDestroy(&p->b);
    free(p->phi);
    int pos = 0, bcell_index;
    poisson_boundary_cell_t* bcell;
    while (int_ptr_unordered_map_next(p->boundary_cells, &pos, &bcell_index, (void**)&bcell))
      free_boundary_cell(bcell);
    int_ptr_unordered_map_clear(p->boundary_cells);
    p->initialized = false;
  }

  // Initialize the linear solver and friends.
  MatCreate(p->comm, &p->A);
  MatSetType(p->A, MATSEQAIJ);
  MatSetOption(p->A, MAT_KEEP_NONZERO_PATTERN, PETSC_TRUE);
//  MatSetOption(p->A, MAT_NEW_NONZERO_LOCATION_ERR, PETSC_TRUE);
//  MatSetOption(p->A, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE);
  MatSetSizes(p->A, p->mesh->num_cells, p->mesh->num_cells, PETSC_DETERMINE, PETSC_DETERMINE);
  VecCreate(p->comm, &p->x);
  VecSetType(p->x, VECSEQ);
  VecSetSizes(p->x, p->mesh->num_cells, PETSC_DECIDE);
  VecCreate(p->comm, &p->b);
  VecSetType(p->b, VECSEQ);
  VecSetSizes(p->b, p->mesh->num_cells, PETSC_DECIDE);
  KSPCreate(p->comm, &p->solver);

  // Initialize the solution vector.
  p->phi = malloc(sizeof(double)*p->mesh->num_cells);

  // Pre-allocate matrix entries.
  PetscInt nz = 0, nnz[p->mesh->num_cells];
  for (int i = 0; i < p->mesh->num_cells; ++i)
    nnz[i] = lin_op_stencil_size(p->L, i);
  MatSeqAIJSetPreallocation(p->A, nz, nnz);

  // Gather information about boundary cells.
  initialize_boundary_cells(p->bcs, p->mesh, p->boundary_cells);

  // If we're independent of time, set up the linear system here.
  if (!p->is_time_dependent)
  {
    set_up_linear_system(p->mesh, p->L, p->rhs, t, p->A, p->b);
    apply_bcs(p->boundary_cells, p->mesh, p->shape, t, p->A, p->b);
  }

  // We simply solve the problem for t = 0.
  poisson_advance((void*)p, t, 0.0);

  // We are now initialized.
  p->initialized = true;
}

static void poisson_plot(void* context, io_interface_t* io, double t, int step)
{
  ASSERT(context != NULL);
  poisson_t* p = (poisson_t*)context;

  io_dataset_t* dataset = io_dataset_new("default", 1, 0);
  io_dataset_write_mesh(dataset, p->mesh);
  io_dataset_write_field(dataset, "phi", p->phi, 1, MESH_CELL);
  io_append_dataset(io, dataset);
}

static void poisson_save(void* context, io_interface_t* io, double t, int step)
{
  ASSERT(context != NULL);
  poisson_t* p = (poisson_t*)context;

  io_dataset_t* dataset = io_dataset_new("default", 1, 0);
  io_dataset_write_mesh(dataset, p->mesh);
  io_dataset_write_field(dataset, "phi", p->phi, 1, MESH_CELL);
  io_append_dataset(io, dataset);
}

static void poisson_dtor(void* ctx)
{
  poisson_t* p = (poisson_t*)ctx;

  // Destroy BC table.
  str_ptr_unordered_map_free(p->bcs);

  if (p->mesh != NULL)
    mesh_free(p->mesh);
  if (p->initialized)
  {
    // Destroy the boundary cell info.
    int pos = 0;
    int bcell;
    void* val;
    while (int_ptr_unordered_map_next(p->boundary_cells, &pos, &bcell, &val))
      free_boundary_cell(val);

    // Destroy other stuff.
    KSPDestroy(&p->solver);
    MatDestroy(&p->A);
    VecDestroy(&p->x);
    VecDestroy(&p->b);
    p->shape = NULL;
    free(p->phi);
  }

  int_ptr_unordered_map_free(p->boundary_cells);
  free(p);
}

model_t* poisson_model_new(options_t* options)
{
  model_vtable vtable = { .init = &poisson_init,
                          .advance = &poisson_advance,
                          .save = &poisson_save,
                          .plot = &poisson_plot,
                          .dtor = &poisson_dtor};
  poisson_t* context = malloc(sizeof(poisson_t));
  context->mesh = NULL;
  context->rhs = NULL;
  context->phi = NULL;
  context->L = NULL;
  context->bcs = str_ptr_unordered_map_new();
  context->shape = poly_ls_shape_new(1, true);
  poly_ls_shape_set_simple_weighting_func(context->shape, 2, 1e-2);
  context->boundary_cells = int_ptr_unordered_map_new();
  context->initialized = false;
  context->comm = MPI_COMM_WORLD;
  model_t* model = model_new("poisson", context, vtable, options);
  model_register_benchmark(model, "laplace_1d", run_laplace_1d, "Laplace's equation in 1D Cartesian coordinates.");
  model_register_benchmark(model, "laplace_1d_2", run_laplace_1d_2, "Laplace's equation in 1D Cartesian coordinates (run in 2D).");
  model_register_benchmark(model, "laplace_1d_3", run_laplace_1d_3, "Laplace's equation in 1D Cartesian coordinates (run in 3D).");
  model_register_benchmark(model, "paraboloid", run_paraboloid, "A paraboloid solution to Poisson's equation (2D).");

  // Set up saver/plotter.
  io_interface_t* saver = silo_io_new(MPI_COMM_SELF, 0, false);
  model_set_saver(model, saver);
  io_interface_t* plotter = vtk_plot_io_new(MPI_COMM_SELF, 0, false);
  model_set_plotter(model, plotter);

  return model;
}


#ifdef __cplusplus
}
#endif

