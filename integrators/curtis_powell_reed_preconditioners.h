// Copyright (c) 2012-2015, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef POLYMEC_CURTIS_POWELL_REED_PRECONDITIONERS_H
#define POLYMEC_CURTIS_POWELL_REED_PRECONDITIONERS_H

#include "core/adj_graph.h"
#include "core/preconditioner.h"
#include "core/sparse_local_matrix.h" // For ILU stuff.

// Curtis-Powell-Reed preconditioners are Newton preconditioners that use the 
// method of Curtis, Powell and Reed to approximate the entries of a Jacobian 
// matrix automatically, given only the residual (or right-hand side) function.

// Creates a block Jacobi preconditioner with the given sparsity graph, 
// number of block rows, and block size. The nature of the sparsity graph 
// (i.e. whether it is a block graph or not) is inferred from the number of 
// block rows and the block size. 
preconditioner_t* block_jacobi_preconditioner_from_function(const char* name, 
                                                            MPI_Comm comm,
                                                            void* context,
                                                            int (*F)(void* context, real_t t, real_t* x, real_t* Fval),
                                                            void (*dtor)(void* context),
                                                            adj_graph_t* sparsity,
                                                            int num_local_block_rows,
                                                            int num_remote_block_rows,
                                                            int block_size);
                                        
// Creates a block Jacobi preconditioner with the given sparsity graph, 
// number of block rows, and block size, appropriate for preconditioning 
// differential-algebraic systems.  
preconditioner_t* block_jacobi_preconditioner_from_dae_function(const char* name, 
                                                                MPI_Comm comm,
                                                                void* context,
                                                                int (*F)(void* context, real_t t, real_t* x, real_t* xdot, real_t* Fval),
                                                                void (*dtor)(void* context),
                                                                adj_graph_t* sparsity,
                                                                int num_local_block_rows,
                                                                int num_remote_block_rows,
                                                                int block_size);

// Creates a block Jacobi preconditioner with the given sparsity graph, 
// number of block rows, and a variable block size (given for each block row 
// by the array entry block_sizes[row]). The nature of the sparsity graph 
// (i.e. whether it is a block graph or not) is inferred from the number of 
// block rows and the block size. The data from the block_sizes array is copied
// into the preconditioner.
preconditioner_t* var_block_jacobi_preconditioner_from_function(const char* name, 
                                                                MPI_Comm comm,
                                                                void* context,
                                                                int (*F)(void* context, real_t t, real_t* x, real_t* Fval),
                                                                void (*dtor)(void* context),
                                                                adj_graph_t* sparsity,
                                                                int num_local_block_rows,
                                                                int num_remote_block_rows,
                                                                int* block_sizes);
                                        
// Creates a block Jacobi preconditioner with the given sparsity graph, 
// number of block rows, and a variable block size, appropriate for 
// preconditioning differential-algebraic systems.  
preconditioner_t* var_block_jacobi_preconditioner_from_dae_function(const char* name, 
                                                                    MPI_Comm comm,
                                                                    void* context,
                                                                    int (*F)(void* context, real_t t, real_t* x, real_t* xdot, real_t* Fval),
                                                                    void (*dtor)(void* context),
                                                                    adj_graph_t* sparsity,
                                                                    int num_local_block_rows,
                                                                    int num_remote_block_rows,
                                                                    int* block_sizes);

// Sparse (Supernode) LU preconditioner.
preconditioner_t* lu_preconditioner_from_function(const char* name, 
                                                  MPI_Comm comm,
                                                  void* context,
                                                  int (*F)(void* context, real_t t, real_t* x, real_t* Fval),
                                                  void (*dtor)(void* context),
                                                  adj_graph_t* sparsity,
                                                  int num_local_block_rows,
                                                  int num_remote_block_rows,
                                                  int block_size);
 
// Sparse (Supernode) LU preconditioner -- differential-algebraic version.
preconditioner_t* lu_preconditioner_from_dae_function(const char* name,
                                                      MPI_Comm comm,
                                                      void* context,
                                                      int (*F)(void* context, real_t t, real_t* x, real_t* xdot, real_t* Fval),
                                                      void (*dtor)(void* context),
                                                      adj_graph_t* sparsity,
                                                      int num_local_block_rows,
                                                      int num_remote_block_rows,
                                                      int block_size);
 
// Sparse (Supernode) LU preconditioner -- variable block size version.
preconditioner_t* var_lu_preconditioner_from_function(const char* name, 
                                                      MPI_Comm comm,
                                                      void* context,
                                                      int (*F)(void* context, real_t t, real_t* x, real_t* Fval),
                                                      void (*dtor)(void* context),
                                                      adj_graph_t* sparsity,
                                                      int num_local_block_rows,
                                                      int num_remote_block_rows,
                                                      int* block_sizes);
 
// Sparse (Supernode) LU preconditioner -- variable block 
// differential-algebraic version.
preconditioner_t* var_lu_preconditioner_from_dae_function(const char* name,
                                                          MPI_Comm comm,
                                                          void* context,
                                                          int (*F)(void* context, real_t t, real_t* x, real_t* xdot, real_t* Fval),
                                                          void (*dtor)(void* context),
                                                          adj_graph_t* sparsity,
                                                          int num_local_block_rows,
                                                          int num_remote_block_rows,
                                                          int* block_sizes);
 
// ILU preconditioner.
preconditioner_t* ilu_preconditioner_from_function(const char* name,
                                                   MPI_Comm comm,
                                                   void* context,
                                                   int (*F)(void* context, real_t t, real_t* x, real_t* Fval),
                                                   void (*dtor)(void* context),
                                                   adj_graph_t* sparsity, 
                                                   int num_local_block_rows,
                                                   int num_remote_block_rows,
                                                   int block_size,
                                                   ilu_params_t* ilu_params);

// ILU preconditioner -- differential-algebraic version.
preconditioner_t* ilu_preconditioner_from_dae_function(const char* name,
                                                       MPI_Comm comm,
                                                       void* context,
                                                       int (*F)(void* context, real_t t, real_t* x, real_t* xdot, real_t* Fval),
                                                       void (*dtor)(void* context),
                                                       adj_graph_t* sparsity, 
                                                       int num_local_block_rows,
                                                       int num_remote_block_rows,
                                                       int block_size,
                                                       ilu_params_t* ilu_params);

// ILU preconditioner -- variable block size version.
preconditioner_t* var_ilu_preconditioner_from_function(const char* name,
                                                       MPI_Comm comm,
                                                       void* context,
                                                       int (*F)(void* context, real_t t, real_t* x, real_t* Fval),
                                                       void (*dtor)(void* context),
                                                       adj_graph_t* sparsity, 
                                                       int num_local_block_rows,
                                                       int num_remote_block_rows,
                                                       int* block_sizes,
                                                       ilu_params_t* ilu_params);

// ILU preconditioner -- variable block size differential-algebraic version.
preconditioner_t* var_ilu_preconditioner_from_dae_function(const char* name,
                                                           MPI_Comm comm,
                                                           void* context,
                                                           int (*F)(void* context, real_t t, real_t* x, real_t* xdot, real_t* Fval),
                                                           void (*dtor)(void* context),
                                                           adj_graph_t* sparsity, 
                                                           int num_local_block_rows,
                                                           int num_remote_block_rows,
                                                           int* block_sizes,
                                                           ilu_params_t* ilu_params);

#endif

