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

#include "core/linear_algebra.h"

// NOTE: All LAPACK functions are implemented within the library, not here.

void matrix_fprintf(double* matrix, int nr, int nc, FILE* stream)
{
  fprintf(stream, "[");
  for (int i = 0; i < nr; ++i)
  {
    for (int j = 0; j < nc; ++j)
      fprintf(stream, "%g ", matrix[nr*j+i]);
    fprintf(stream, "; ");
  }
  fprintf(stream, "]");
}

void vector_fprintf(double* vec, int nr, FILE* stream)
{
  fprintf(stream, "[");
  for (int i = 0; i < nr; ++i)
    fprintf(stream, "%g ", vec[i]);
  fprintf(stream, "]");
}

double matrix2_det(double* matrix)
{
  return matrix[0]*matrix[3] - matrix[1]*matrix[2];
}

double matrix3_det(double* matrix)
{
  return matrix[0]*(matrix[4]*matrix[8] - matrix[5]*matrix[7]) -
         matrix[3]*(matrix[1]*matrix[8] - matrix[2]*matrix[7]) + 
         matrix[6]*(matrix[1]*matrix[5] - matrix[2]*matrix[4]);
}

void solve_2x2(double* A, double* b, double* x)
{
  double b0 = b[0], b1 = b[1];
  double inv_det_A = 1.0 / matrix2_det(A);
  x[0] = inv_det_A * ( A[3]*b0 - A[2]*b1);
  x[1] = inv_det_A * (-A[1]*b0 + A[0]*b1);
}

void solve_3x3(double* A, double* b, double* x)
{
  double b0 = b[0], b1 = b[1], b2 = b[2];

  // x = Ainv * b.
  double inv_det_A = 1.0 / matrix3_det(A);

  x[0] = inv_det_A * 
         ((A[8]*A[4]-A[5]*A[7]) * b0 - 
          (A[8]*A[3]-A[5]*A[6]) * b1 + 
          (A[7]*A[3]-A[4]*A[6]) * b2);

  x[1] = inv_det_A * 
         (-(A[8]*A[1]-A[2]*A[7]) * b0 +
           (A[8]*A[0]-A[2]*A[6]) * b1 -
           (A[7]*A[0]-A[1]*A[6]) * b2);

  x[2] = inv_det_A * 
         ((A[5]*A[1]-A[2]*A[4]) * b0 -
          (A[5]*A[0]-A[2]*A[3]) * b1 +
          (A[4]*A[0]-A[1]*A[3]) * b2);
}

