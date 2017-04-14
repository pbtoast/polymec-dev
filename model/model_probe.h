// Copyright (c) 2012-2017, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef POLYMEC_MODEL_PROBE_H
#define POLYMEC_MODEL_PROBE_H

#include "core/polymec.h"
#include "core/unordered_map.h"

// A model probe is a virtual instrument that acquires data from a model.
typedef struct model_probe_t model_probe_t;

// This virtual table must be implemented by any model probe.
typedef struct 
{
  // Acquire data from a model. The data's rank and shape are given, and 
  // the data is placed into the data array.
  void (*acquire)(void* context, real_t t, int rank, size_t* shape, real_t* data); 

  // Destructor.
  void (*dtor)(void* context);
} model_probe_vtable;

// Creates an instance of a model probe that acquires a quantity of the given 
// name, placing its data into an array large enough to store all of its 
// contents. Here, datum_size is the number of real numbers required to store 
// a datum.
model_probe_t* model_probe_new(const char* name, 
                               int array_rank,
                               size_t* array_shape,
                               void* context, 
                               model_probe_vtable vtable);

// Destroys the probe.
void model_probe_free(model_probe_t* probe);

// Returns the name of the probe.
char* model_probe_name(model_probe_t* probe);

// Returns the rank of the data acquired by this probe.
int model_probe_data_rank(model_probe_t* probe);

// Returns the shape (a rank-sized array) of the data acquired by this probe.
size_t* model_probe_data_shape(model_probe_t* probe);

// Allocates and returns an array of sufficient size to hold an aquisition.
real_t* model_probe_new_array(model_probe_t* probe);

// Acquires the quantity at the given time t, placing it into data.
void model_probe_acquire(model_probe_t* probe, real_t t, real_t* data);

// Adds a callback function that is called when this probe acquires data, and the 
// context pointer with which the function is called. A probe cannot assume ownership
// of a context pointer, so the pointer must outlive the probe.
void model_probe_add_callback(model_probe_t* probe, 
                              void* context, 
                              void (*callback)(void* context,
                                               real_t t, 
                                               int rank,
                                               size_t* shape,
                                               real_t* data));

#endif

