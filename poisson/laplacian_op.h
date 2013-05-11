#ifndef POLYMEC_LAPLACIAN_OP_H
#define POLYMEC_LAPLACIAN_OP_H

#include "core/lin_op.h"

// Returns a 2nd-order, finite-volume, cell-centered Laplacian operator.
lin_op_t* laplacian_op_new(mesh_t* mesh);

#endif
