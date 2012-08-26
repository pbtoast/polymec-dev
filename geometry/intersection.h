#ifndef ARBI_INTERSECTION_H
#define ARBI_INTERSECTION_H

#include "core/sp_func.h"

#ifdef __cplusplus
extern "C" {
#endif

// This signed distance function represents the intersection of the set of 
// given surfaces represented by signed distance functions.
sp_func_t* intersection_new(sp_func_t** surfaces, int num_surfaces);

#ifdef __cplusplus
}
#endif

#endif

