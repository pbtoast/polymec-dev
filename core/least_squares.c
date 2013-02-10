#include <stdlib.h>
#include <string.h>
#include <gc/gc.h>
#include "core/least_squares.h"
#include "core/linear_algebra.h"

#ifdef __cplusplus
extern "C" {
#endif

struct multi_index_t 
{
  int p, x_order, y_order, z_order;
  int offset; // Offset in flattened index space.
};

multi_index_t* multi_index_new(int p)
{
  ASSERT(p >= 0);
  ASSERT(p < 4);
  multi_index_t* m = GC_MALLOC(sizeof(multi_index_t));
  m->p = p;
  m->x_order = m->y_order = m->z_order = 0;
  m->offset = 0;
  return m;
}

static int multi_index_sizes[] = {1, 4, 10, 18};

bool multi_index_next(multi_index_t* m, int* x_order, int* y_order, int* z_order)
{
  ASSERT(m->p >= 0);
  ASSERT(m->p < 4);
  if (m->offset == -1)
    return false;
  if (m->p == 0)
  {
    *x_order = m->x_order;
    *y_order = m->y_order;
    *z_order = m->z_order;
    m->offset = -1;
  }
  if (m->p == 1)
  {
    static const multi_index_t multi_index[] = 
    {
      {.p = 1, .x_order = 0, .y_order = 0, .z_order = 0, .offset = 0}, 
      {.p = 1, .x_order = 1, .y_order = 0, .z_order = 0, .offset = 1}, 
      {.p = 1, .x_order = 0, .y_order = 1, .z_order = 0, .offset = 2}, 
      {.p = 1, .x_order = 0, .y_order = 0, .z_order = 1, .offset = 3},
      {.p = 1, .offset = -1}
    };
    *x_order = m->x_order;
    *y_order = m->y_order;
    *z_order = m->z_order;
    *m = multi_index[m->offset+1];
  }
  else if (m->p == 2)
  {
    static const multi_index_t multi_index[] = 
    {
      {.p = 2, .x_order = 0, .y_order = 0, .z_order = 0, .offset = 0}, 
      {.p = 2, .x_order = 1, .y_order = 0, .z_order = 0, .offset = 1}, 
      {.p = 2, .x_order = 0, .y_order = 1, .z_order = 0, .offset = 2}, 
      {.p = 2, .x_order = 0, .y_order = 0, .z_order = 1, .offset = 3},
      {.p = 2, .x_order = 2, .y_order = 0, .z_order = 0, .offset = 4},
      {.p = 2, .x_order = 1, .y_order = 1, .z_order = 0, .offset = 5},
      {.p = 2, .x_order = 1, .y_order = 0, .z_order = 1, .offset = 6},
      {.p = 2, .x_order = 0, .y_order = 2, .z_order = 0, .offset = 7},
      {.p = 2, .x_order = 0, .y_order = 1, .z_order = 1, .offset = 8},
      {.p = 2, .x_order = 0, .y_order = 0, .z_order = 2, .offset = 9},
      {.p = 1, .offset = -1}
    };
    *x_order = m->x_order;
    *y_order = m->y_order;
    *z_order = m->z_order;
    *m = multi_index[m->offset+1];
  }
  else if (m->p == 3)
  {
    static const multi_index_t multi_index[] = 
    {
      {.p = 3, .x_order = 0, .y_order = 0, .z_order = 0, .offset = 0}, 
      {.p = 3, .x_order = 1, .y_order = 0, .z_order = 0, .offset = 1}, 
      {.p = 3, .x_order = 0, .y_order = 1, .z_order = 0, .offset = 2}, 
      {.p = 3, .x_order = 0, .y_order = 0, .z_order = 1, .offset = 3},
      {.p = 3, .x_order = 2, .y_order = 0, .z_order = 0, .offset = 4},
      {.p = 3, .x_order = 1, .y_order = 1, .z_order = 0, .offset = 5},
      {.p = 3, .x_order = 1, .y_order = 0, .z_order = 1, .offset = 6},
      {.p = 3, .x_order = 0, .y_order = 2, .z_order = 0, .offset = 7},
      {.p = 3, .x_order = 0, .y_order = 1, .z_order = 1, .offset = 8},
      {.p = 3, .x_order = 0, .y_order = 0, .z_order = 2, .offset = 9},
      {.p = 3, .x_order = 3, .y_order = 0, .z_order = 0, .offset = 10},
      {.p = 3, .x_order = 2, .y_order = 1, .z_order = 0, .offset = 11},
      {.p = 3, .x_order = 2, .y_order = 0, .z_order = 1, .offset = 12},
      {.p = 3, .x_order = 1, .y_order = 2, .z_order = 0, .offset = 13},
      {.p = 3, .x_order = 1, .y_order = 1, .z_order = 1, .offset = 14},
      {.p = 3, .x_order = 0, .y_order = 3, .z_order = 0, .offset = 15},
      {.p = 3, .x_order = 0, .y_order = 2, .z_order = 1, .offset = 16},
      {.p = 3, .x_order = 0, .y_order = 1, .z_order = 2, .offset = 17},
      {.p = 1, .offset = -1}
    };
    *x_order = m->x_order;
    *y_order = m->y_order;
    *z_order = m->z_order;
    *m = multi_index[m->offset+1];
  }
  return true; 
}

void multi_index_reset(multi_index_t* m)
{
  m->offset = 0;
  m->x_order = m->y_order = m->z_order = 0;
}

int multi_index_order(multi_index_t* m)
{
  return m->p;
}

int multi_index_size(multi_index_t* m)
{
  return multi_index_sizes[m->p];
}

int poly_ls_basis_size(int p)
{
  return multi_index_sizes[p];
}

void compute_poly_ls_basis_vector(int p, point_t* point, double* basis)
{
  multi_index_t* m = multi_index_new(p);
  int i = 0, x, y, z;
  while (multi_index_next(m, &x, &y, &z))
    basis[i++] = pow(point->x, x)*pow(point->y, y)*pow(point->z, z);
  m = NULL;
}

void compute_poly_ls_basis_gradient(int p, point_t* point, vector_t* gradients)
{
  multi_index_t* m = multi_index_new(p);
  int i = 0, x, y, z;
  while (multi_index_next(m, &x, &y, &z))
  {
    gradients[i].x = (x == 0) ? 0.0 : x*pow(point->x, x-1)*pow(point->y, y)*pow(point->z, z);
    gradients[i].y = (y == 0) ? 0.0 : pow(point->x, x)*y*pow(point->y, y-1)*pow(point->z, z);
    gradients[i++].z = (z == 0) ? 0.0 : pow(point->x, x)*pow(point->y, y)*z*pow(point->z, z-1);
  }
  m = NULL;
}

void compute_poly_ls_system(int p, point_t* x0, point_t* points, int num_points, 
                            double* data, double* moment_matrix, double* rhs)
{
  ASSERT(p >= 0);
  ASSERT(p < 4);
  ASSERT(num_points >= multi_index_sizes[p]);
  ASSERT(moment_matrix != NULL);
  ASSERT(rhs != NULL);
  int size = multi_index_sizes[p];
  double basis[size];

  // Zero the system.
  memset(moment_matrix, 0, sizeof(double)*size*size);
  memset(rhs, 0, sizeof(double)*size);
 
  for (int n = 0; n < num_points; ++n)
  {
    if (x0 != NULL)
    {
      point_t y = {.x = points[n].x - x0->x, 
                   .y = points[n].y - x0->y,
                   .z = points[n].z - x0->z};
      compute_poly_ls_basis_vector(p, &y, basis);
    }
    else
    {
      compute_poly_ls_basis_vector(p, &points[n], basis);
    }
    for (int i = 0; i < size; ++i)
    {
      for (int j = 0; j < size; ++j)
        moment_matrix[size*j+i] += basis[i]*basis[j];
      rhs[i] += basis[i]*data[n];
    }
  }
}

void compute_weighted_poly_ls_system(int p, ls_weighting_func_t W, point_t* x0, point_t* points, int num_points, 
                                     double* data, double* moment_matrix, double* rhs)
{
  ASSERT(p >= 0);
  ASSERT(p < 4);
  ASSERT(moment_matrix != NULL);
  ASSERT(rhs != NULL);
  int size = multi_index_sizes[p];
  double basis[size];

  memset(moment_matrix, 0, sizeof(double)*size*size);
  memset(rhs, 0, sizeof(double)*size);
 
  // Compute the average distance between the points. This will serve as 
  // our spatial scale length, h.
  double h = 0.0;
  for (int n = 0; n < num_points; ++n)
    for (int l = n+1; l < num_points; ++l)
      h += point_distance(&points[n], &points[l]);
  h /= (num_points*(num_points+1)/2 - num_points);

  for (int n = 0; n < num_points; ++n)
  {
    double d;
    if (x0 != NULL)
    {
      point_t y = {.x = points[n].x - x0->x, 
                   .y = points[n].y - x0->y,
                   .z = points[n].z - x0->z};
      compute_poly_ls_basis_vector(p, &y, basis);
      d = y.x*y.x + y.y*y.y + y.z*y.z;
    }
    else
    {
      compute_poly_ls_basis_vector(p, &points[n], basis);
      d = points[n].x*points[n].x + points[n].y*points[n].y + points[n].z*points[n].z;
    }
    double Wd;
    vector_t gradWd;
    W(NULL, &points[n], x0, h, &Wd, &gradWd);
    for (int i = 0; i < size; ++i)
    {
      for (int j = 0; j < size; ++j)
        moment_matrix[size*j+i] += Wd*basis[i]*basis[j];
      rhs[i] += Wd*basis[i]*data[n];
    }
  }
}

// Shape function basis.
struct poly_ls_shape_t 
{
  int p; // Order of basis.
  bool compute_gradients; // Compute gradients, or no?
  int dim; // Dimension of basis.
  double *domain_basis; // Polynomial basis, calculated during set_domain().
  int num_points; // Number of points in domain.
  point_t* points; // Points in domain.
  point_t x0; // Origin.
  double h; // Smoothing length scale.
  ls_weighting_func_t weighting_func; // Weighting function.
  void* w_context; // Context pointer for weighting function.
  void (*w_dtor)(void*); // Destructor for weighting function context pointer.
};

static void poly_ls_shape_free(void* context, void* dummy)
{
  poly_ls_shape_t* N = (poly_ls_shape_t*)context;
  if ((N->w_context != NULL) && (N->w_dtor != NULL))
    (*N->w_dtor)(N->w_context);
  if (N->points != NULL)
    free(N->points);
  if (N->domain_basis != NULL)
    free(N->domain_basis);
  free(N);
}

static void no_weighting_func(void* context, point_t* x, point_t* x0, double h, double* W, vector_t* gradient)
{
  *W = 1.0;
  gradient->x = gradient->y = gradient->z = 0.0;
//  polymec_error("No weighting function has been set for this LS shape function.");
}

poly_ls_shape_t* poly_ls_shape_new(int p, bool compute_gradients)
{
  ASSERT(p >= 0);
  ASSERT(p < 4);
  poly_ls_shape_t* N = GC_MALLOC(sizeof(poly_ls_shape_t));
  N->p = p;
  N->compute_gradients = compute_gradients;
  N->weighting_func = &no_weighting_func;
  N->w_context = NULL;
  N->w_dtor = NULL;
  N->dim = poly_ls_basis_size(p);
  N->domain_basis = NULL;
  N->num_points = 0;
  N->points = NULL;
  N->h = 0.0;
  GC_register_finalizer(N, &poly_ls_shape_free, N, NULL, NULL);
  return N;
}

void poly_ls_shape_set_domain(poly_ls_shape_t* N, point_t* x0, point_t* points, int num_points)
{
  int dim = N->dim;
  if (num_points != N->num_points)
  {
    N->num_points = num_points;
    N->points = realloc(N->points, sizeof(point_t)*num_points);
    N->domain_basis = realloc(N->domain_basis, sizeof(double)*dim*num_points);
  }
  N->x0.x = x0->x;
  N->x0.y = x0->y;
  N->x0.z = x0->z;
  memcpy(N->points, points, sizeof(point_t)*num_points);

  for (int n = 0; n < num_points; ++n)
  {
    // Expand about x0.
    point_t y = {.x = points[n].x - x0->x, 
                 .y = points[n].y - x0->y,
                 .z = points[n].z - x0->z};
    compute_poly_ls_basis_vector(N->p, &y, &N->domain_basis[dim*n]);
  }

  // Compute the average distance between the points. This will serve as 
  // our spatial scale length, h.
  N->h = 0.0;
  for (int n = 0; n < num_points; ++n)
    for (int l = n+1; l < num_points; ++l)
      N->h += point_distance(&N->points[n], &N->points[l]);
  N->h /= (num_points*(num_points+1)/2 - num_points);
}

void poly_ls_shape_compute(poly_ls_shape_t* N, point_t* x, double* values)
{
  poly_ls_shape_compute_gradients(N, x, values, NULL);
}

void poly_ls_shape_compute_gradients(poly_ls_shape_t* N, point_t* x, double* values, vector_t* gradients)
{
  ASSERT((gradients == NULL) || N->compute_gradients);
  int dim = N->dim;
  int num_points = N->num_points;

  // Compute the weights and their gradients at x.
  double W[num_points];
  vector_t grad_W[num_points];
  for (int n = 0; n < num_points; ++n)
    N->weighting_func(N->w_context, x, &N->points[n], N->h, &W[n], &grad_W[n]);

  // Compute the moment matrix A.
  double A[dim*dim], AinvB[dim*num_points];
//printf("x0 = %g %g %g\n", N->x0.x, N->x0.y, N->x0.z);
//printf("points = ");
//for (int n = 0; n < N->num_points; ++n)
//printf("%g %g %g  ", N->points[n].x, N->points[n].y, N->points[n].z);
//printf("\n");
  memset(A, 0, sizeof(double)*dim*dim);
  for (int n = 0; n < num_points; ++n)
  {
    for (int i = 0; i < dim; ++i)
    {
      AinvB[dim*n+i] = W[n] * N->domain_basis[dim*n+i];
      for (int j = 0; j < dim; ++j)
        A[dim*j+i] += W[n] * N->domain_basis[dim*n+i] * N->domain_basis[dim*n+j];
    }
  }
//  matrix_fprintf(A, dim, dim, stdout);
//  printf("\n");

  // Factor the moment matrix.
  int pivot[dim], info;
  dgetrf(&dim, &dim, A, &dim, pivot, &info);
  ASSERT(info == 0);

  // Compute Ainv * B.
  char no_trans = 'N';
  dgetrs(&no_trans, &dim, &num_points, A, &dim, pivot, AinvB, &dim, &info);
  ASSERT(info == 0);

  // values^T = basis^T * Ainv * B (or values = (Ainv * B)^T * basis.)
  double alpha = 1.0, beta = 0.0;
  int one = 1;
  char trans = 'T';
  double basis[dim];
  point_t y = {.x = x->x - N->x0.x, .y = x->y - N->x0.y, .z = x->z - N->x0.z};
  compute_poly_ls_basis_vector(N->p, &y, basis);
  //printf("y = %g %g %g, basis = ", y.x, y.y, y.z);
  //for (int i = 0; i < dim; ++i)
  //printf("%g ", basis[i]);
  //printf("\n");
  dgemv(&trans, &dim, &num_points, &alpha, AinvB, &dim, basis, &one, &beta, values, &one);

  // If we are in the business of computing gradients, compute the 
  // partial derivatives of Ainv * B.
  if (N->compute_gradients && (gradients != NULL))
  {
    // Compute the derivative of A inverse. We'll need the derivatives of 
    // A and B first.
    double dAdx[dim*dim], dAdy[dim*dim], dAdz[dim*dim],
           dBdx[dim*num_points], dBdy[dim*num_points], dBdz[dim*num_points];
    memset(dAdx, 0, sizeof(double)*dim*dim);
    memset(dAdy, 0, sizeof(double)*dim*dim);
    memset(dAdz, 0, sizeof(double)*dim*dim);
    for (int n = 0; n < num_points; ++n)
    {
      for (int i = 0; i < dim; ++i)
      {
        dBdx[dim*n+i] = grad_W[n].x*N->domain_basis[dim*n+i];
        dBdy[dim*n+i] = grad_W[n].y*N->domain_basis[dim*n+i];
        dBdz[dim*n+i] = grad_W[n].z*N->domain_basis[dim*n+i];
        for (int j = 0; j < dim; ++j)
        {
          dAdx[dim*j+i] += grad_W[n].x*N->domain_basis[dim*n+i]*N->domain_basis[dim*n+j];
          dAdy[dim*j+i] += grad_W[n].y*N->domain_basis[dim*n+i]*N->domain_basis[dim*n+j];
          dAdz[dim*j+i] += grad_W[n].z*N->domain_basis[dim*n+i]*N->domain_basis[dim*n+j];
        }
      }
    }

    // The partial derivatives of A inverse are:
    // d(Ainv) = -Ainv * dA * Ainv, so
    // d(Ainv * B) = -Ainv * dA * Ainv * B + Ainv * dB
    //             = Ainv * (-dA * Ainv * B + dB).

    // We left-multiply Ainv*B by the gradient of A, placing the results 
    // in dAinvBdx, dAinvBdy, and dAinvBdz.
    double alpha = 1.0, beta = 0.0;
    double dAinvBdx[dim*num_points], dAinvBdy[dim*num_points], dAinvBdz[dim*num_points];
    dgemm(&no_trans, &no_trans, &dim, &num_points, &dim, &alpha, 
        dAdx, &dim, AinvB, &dim, &beta, dAinvBdx, &dim);
    dgemm(&no_trans, &no_trans, &dim, &num_points, &dim, &alpha, 
        dAdy, &dim, AinvB, &dim, &beta, dAinvBdy, &dim);
    dgemm(&no_trans, &no_trans, &dim, &num_points, &dim, &alpha, 
        dAdz, &dim, AinvB, &dim, &beta, dAinvBdz, &dim);

    // Flip the sign of dA * Ainv * B, and add dB.
    for (int i = 0; i < dim*num_points; ++i)
    {
      dAinvBdx[i] = -dAinvBdx[i] + dBdx[i];
      dAinvBdy[i] = -dAinvBdy[i] + dBdy[i];
      dAinvBdz[i] = -dAinvBdz[i] + dBdz[i];
    }

    // Now "left-multiply by Ainv" by solving the equation (e.g.)
    // A * (dAinvBdx) = (-dA * Ainv * B + dB).
    dgetrs(&no_trans, &dim, &num_points, A, &dim, pivot, dAinvBdx, &dim, &info);
    ASSERT(info == 0);
    dgetrs(&no_trans, &dim, &num_points, A, &dim, pivot, dAinvBdy, &dim, &info);
    ASSERT(info == 0);
    dgetrs(&no_trans, &dim, &num_points, A, &dim, pivot, dAinvBdz, &dim, &info);
    ASSERT(info == 0);

    // Now compute the gradients.

    // 1st term: gradient of basis, dotted with Ainv * B.
    vector_t basis_grads[dim];
    compute_poly_ls_basis_gradient(N->p, &y, basis_grads);
    double dpdx[dim], dpdy[dim], dpdz[dim];
    for (int i = 0; i < dim; ++i)
    {
      dpdx[i] = basis_grads[i].x;
      dpdy[i] = basis_grads[i].y;
      dpdz[i] = basis_grads[i].z;
    }
    double dpdx_AinvB[num_points], dpdy_AinvB[num_points], dpdz_AinvB[num_points];
    dgemv(&trans, &dim, &num_points, &alpha, AinvB, &dim, dpdx, &one, &beta, dpdx_AinvB, &one);
    dgemv(&trans, &dim, &num_points, &alpha, AinvB, &dim, dpdy, &one, &beta, dpdy_AinvB, &one);
    dgemv(&trans, &dim, &num_points, &alpha, AinvB, &dim, dpdz, &one, &beta, dpdz_AinvB, &one);

    // Second term: basis dotted with gradient of Ainv * B.
    double p_dAinvBdx[num_points], p_dAinvBdy[num_points], p_dAinvBdz[num_points];
    dgemv(&trans, &dim, &num_points, &alpha, dAinvBdx, &dim, basis, &one, &beta, p_dAinvBdx, &one);
    dgemv(&trans, &dim, &num_points, &alpha, dAinvBdy, &dim, basis, &one, &beta, p_dAinvBdy, &one);
    dgemv(&trans, &dim, &num_points, &alpha, dAinvBdz, &dim, basis, &one, &beta, p_dAinvBdz, &one);

    // Gradients are the sum of these terms.
    for (int i = 0; i < num_points; ++i)
    {
      gradients[i].x = dpdx_AinvB[i] + p_dAinvBdx[i];
      gradients[i].y = dpdy_AinvB[i] + p_dAinvBdy[i];
      gradients[i].z = dpdz_AinvB[i] + p_dAinvBdz[i];
    }
  }
}

void poly_ls_shape_compute_ghost_transform(poly_ls_shape_t* N, int* ghost_indices, int num_ghosts,
                                           point_t* constraint_points, 
                                           double* a, double* b, double* c, double* d, double* e,
                                           double* A, double* B)
{
  ASSERT(N->p > 0); // Constraints cannot be applied to constants.
  ASSERT(N->compute_gradients);
  ASSERT(ghost_indices != NULL);
  ASSERT(num_ghosts < N->num_points);
  ASSERT(constraint_points != NULL);
  ASSERT(a != NULL);
  ASSERT(b != NULL);
  ASSERT(c != NULL);
  ASSERT(d != NULL);
  ASSERT(e != NULL);
  ASSERT(A != NULL);
  ASSERT(B != NULL);

  // Set up the constraint matrices.
  double amat[num_ghosts*num_ghosts];
  for (int i = 0; i < num_ghosts; ++i)
  {
    // Compute the shape functions at xi.
    double N_vals[N->num_points];
    vector_t N_grads[N->num_points];
    poly_ls_shape_compute_gradients(N, &constraint_points[i], N_vals, N_grads);
//printf("points = ");
//for (int n = 0; n < N->num_points; ++n)
//printf("%g %g %g  ", N->points[n].x, N->points[n].y, N->points[n].z);
//printf("\n");
//printf("constraint point (%d) is %g %g %g\n", constraint_indices[i], N->points[constraint_indices[i]].x, N->points[constraint_indices[i]].y, N->points[constraint_indices[i]].z);
//printf("N = ");
//for (int n = 0; n < N->num_points; ++n)
//printf("%g ", N_vals[n]);
//printf("\n");
//printf("grad N = ");
//for (int n = 0; n < N->num_points; ++n)
//printf("%g %g %g  ", N_grads[n].x, N_grads[n].y, N_grads[n].z);
//printf("\n");
//printf("a b c d e = %g %g %g %g %g\n", a[i], b[i], c[i], d[i], e[i]);

    // Now set up the left and right hand sides of the equation for the constraint.
    for (int j = 0; j < N->num_points; ++j)
    {
      bool constrained = false;
      int k = 0;
      for (; k < num_ghosts; ++k)
      {
        if (j == ghost_indices[k]) 
        {
          constrained = true;
          break;
        }
      }
      if (constrained) 
      {
        amat[num_ghosts*k+i] = a[i]*N_vals[j] + b[i]*N_grads[j].x + c[i]*N_grads[j].y + d[i]*N_grads[j].z;
        A[num_ghosts*j+i] = 0.0;
      }
      else
      {
        A[num_ghosts*j+i] = -a[i]*N_vals[j] - b[i]*N_grads[j].x - c[i]*N_grads[j].y - d[i]*N_grads[j].z;
      }
    }
  }

  // Compute A by solving the linear system.
  int pivot[num_ghosts], info;
//  printf("amat = ");
//  for (int i = 0; i < num_constraints*num_constraints; ++i)
//    printf("%g ", amat[i]);
//  printf("\n");
  dgetrf(&num_ghosts, &num_ghosts, amat, &num_ghosts, pivot, &info);
  ASSERT(info == 0);
  char no_trans = 'N';
  dgetrs(&no_trans, &num_ghosts, &N->num_points, amat, &num_ghosts, pivot, A, &num_ghosts, &info);
  ASSERT(info == 0);

  // Compute B = amatinv * e.
  int one = 1;
  memcpy(B, e, sizeof(double)*num_ghosts);
  dgetrs(&no_trans, &num_ghosts, &one, amat, &num_ghosts, pivot, B, &num_ghosts, &info);
  ASSERT(info == 0);
}

typedef struct 
{
  int A;
  double B;
} simple_weighting_func_params_t;

static void simple_weighting_func(void* context, point_t* x, point_t* x0, double h, double* W, vector_t* gradient)
{
  simple_weighting_func_params_t* params = (simple_weighting_func_params_t*)context;
  double D = point_distance(x, x0)/h;
  *W = 1.0 / (pow(D, params->A) + pow(params->B, params->A));
  if (D == 0.0)
  {
    gradient->x = gradient->y = gradient->z = 0.0;
  }
  else
  {
    double dDdx = (x->x - x0->x) / D, 
           dDdy = (x->y - x0->y) / D, 
           dDdz = (x->z - x0->z) / D;
    double deriv_term = -(*W)*(*W) * params->A * pow(D, params->A-1);
    gradient->x = deriv_term * dDdx;
    gradient->y = deriv_term * dDdy;
    gradient->z = deriv_term * dDdz;
  }
}

void poly_ls_shape_set_simple_weighting_func(poly_ls_shape_t* N, int A, double B)
{
  ASSERT(A > 0);
  ASSERT(B > 0);
  N->weighting_func = &simple_weighting_func;
  simple_weighting_func_params_t* params = malloc(sizeof(simple_weighting_func_params_t));
  params->A = A;
  params->B = B;
  N->w_context = params;
  N->w_dtor = &free;
}

void linear_regression(double* x, double* y, int N, double* A, double* B, double* sigma)
{
  ASSERT(N > 2);
  double sumXY = 0.0, sumX = 0.0, sumY = 0.0, sumX2 = 0.0, sumY2 = 0.0;
  for (int i = 0; i < N; ++i)
  {
    sumX += x[i];
    sumX2 += x[i]*x[i];
    sumY += y[i];
    sumY2 += y[i]*y[i];
    sumXY += x[i]*y[i];
  }
  *A = (N * sumXY - sumX*sumY) / (N * sumX2 - sumX*sumX);
  *B = (sumY - *A * sumX) / N;
  double SSE = 0.0;
  for (int i = 0; i < N; ++i)
  {
    double e = (*A) * x[i] + (*B) - y[i];
    SSE += e*e;
  }
  *sigma = SSE / (N - 2);
}

#ifdef __cplusplus
}
#endif

