// Copyright (c) 2012-2019, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "solvers/krylov_solver.h"
#include "core/file_utils.h"

extern krylov_factory_t* petsc_krylov_factory_32(const char* petsc_library);
extern krylov_factory_t* petsc_krylov_factory_64(const char* petsc_library);

krylov_factory_t* petsc_krylov_factory(const char* petsc_library,
                                       bool use_64_bit_indices)
{
  if (!file_exists(petsc_library))
  {
    log_urgent("PETSc library %s not found.", petsc_library);
    return NULL;
  }
  if (use_64_bit_indices)
    return petsc_krylov_factory_64(petsc_library);
  else
    return petsc_krylov_factory_32(petsc_library);
}

