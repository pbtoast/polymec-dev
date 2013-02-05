#include "core/hypre_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

HYPRE_IJMatrix HYPRE_IJMatrixNew(index_space_t* index_space)
{
  HYPRE_IJMatrix A;
  MPI_Comm comm = index_space->comm;
  int low = index_space->low;
  int high = index_space->high;
  int err = HYPRE_IJMatrixCreate(comm, low, high-1, low, high-1, &A);
  ASSERT(err == 0);
  err = HYPRE_IJMatrixSetObjectType(A, HYPRE_PARCSR);
  ASSERT(err == 0);

#ifndef NDEBUG
  // Print diagnostic information.
  HYPRE_IJMatrixSetPrintLevel(A, 1);
#endif
  return A;
}

void HYPRE_IJMatrixSetRowSizesFromTable(HYPRE_IJMatrix matrix, index_space_t* index_space, double_table_t* table)
{
  int N = index_space->high - index_space->low;
  int nnz[N];
  int rpos = 0, i;
  double_table_row_t* row_data;
  while (double_table_next_row(table, &rpos, &i, &row_data))
    nnz[i] = row_data->size;
  int err = HYPRE_IJMatrixSetRowSizes(matrix, nnz);
  ASSERT(err == 0);
}

static void HYPRE_IJMatrixModifyValuesFromTable(HYPRE_IJMatrix matrix, index_space_t* index_space, double_table_t* table, int (*modify_values)(HYPRE_IJMatrix, int, int*, const int*, const int*, const double*))
{
  int err = HYPRE_IJMatrixInitialize(matrix);
  ASSERT(err == 0);
  int rpos = 0, i;
  double_table_row_t* row_data;
  while (double_table_next_row(table, &rpos, &i, &row_data))
  {
    int cpos = 0, j, num_cols = row_data->size;
    int indices[num_cols], offset = 0;
    double values[num_cols], Aij;
    while (double_table_row_next(row_data, &cpos, &j, &Aij))
    {
      indices[offset] = j;
      values[offset] = Aij;
      offset++;
    }
    err = modify_values(matrix, 1, &num_cols, &i, indices, values);
    ASSERT(err == 0);
  }
  err = HYPRE_IJMatrixAssemble(matrix);
  ASSERT(err == 0);
}

void HYPRE_IJMatrixSetValuesFromTable(HYPRE_IJMatrix matrix, index_space_t* index_space, double_table_t* table)
{
  HYPRE_IJMatrixModifyValuesFromTable(matrix, index_space, table, HYPRE_IJMatrixSetValues);
}

void HYPRE_IJMatrixAddToValuesFromTable(HYPRE_IJMatrix matrix, index_space_t* index_space, double_table_t* table)
{
  HYPRE_IJMatrixModifyValuesFromTable(matrix, index_space, table, HYPRE_IJMatrixAddToValues);
}

HYPRE_IJVector HYPRE_IJVectorNew(index_space_t* index_space)
{
  HYPRE_IJVector x;
  MPI_Comm comm = index_space->comm;
  int low = index_space->low;
  int high = index_space->high;
  int err = HYPRE_IJVectorCreate(comm, low, high-1, &x);
  ASSERT(err == 0);
  err = HYPRE_IJVectorSetObjectType(x, HYPRE_PARCSR);
  ASSERT(err == 0);

  // Initialize the vector to zero.
  int N = index_space->high - index_space->low;
  int indices[N];
  double values[N];
  for (int i = 0; i < N; ++i)
  {
    indices[i] = index_space->low + i;
    values[i] = 0.0;
  }
  err = HYPRE_IJVectorInitialize(x);
  ASSERT(err == 0);
  HYPRE_IJVectorSetValues(x, N, indices, values);
  err = HYPRE_IJVectorAssemble(x);
  ASSERT(err == 0);

  return x;
}

static void HYPRE_IJVectorModifyValuesFromArray(HYPRE_IJVector vector, index_space_t* index_space, double* array, int (*modify_values)(HYPRE_IJVector, int, const int*, const double*))
{
  HYPRE_IJVectorInitialize(vector);
  int N = index_space->high - index_space->low;
  int indices[N];
  for (int i = 0; i < N; ++i)
    indices[i] = index_space->low + i;
  modify_values(vector, N, indices, array);
  HYPRE_IJVectorAssemble(vector);
}

void HYPRE_IJVectorSetValuesFromArray(HYPRE_IJVector vector, index_space_t* index_space, double* array)
{
  HYPRE_IJVectorModifyValuesFromArray(vector, index_space, array, HYPRE_IJVectorSetValues);
}

void HYPRE_IJVectorAddToValuesFromArray(HYPRE_IJVector vector, index_space_t* index_space, double* array)
{
  HYPRE_IJVectorModifyValuesFromArray(vector, index_space, array, HYPRE_IJVectorAddToValues);
}

void HYPRE_IJVectorGetValuesToArray(HYPRE_IJVector vector, index_space_t* index_space, double* array)
{
  int N = index_space->high - index_space->low;
  int indices[N];
  for (int i = 0; i < N; ++i)
    indices[i] = index_space->low + i;
  HYPRE_IJVectorGetValues(vector, N, indices, array);
}

#ifdef __cplusplus
}
#endif

