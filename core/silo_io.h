#ifndef ARBI_SILO_IO_H
#define ARBI_SILO_IO_H

#include "core/io.h"

#ifdef __cplusplus
extern "C" {
#endif

// Creates a Silo I/O interface.
io_interface_t* silo_io_new(MPI_Comm comm, int num_files, int mpi_tag);

#ifdef __cplusplus
}
#endif

#endif

