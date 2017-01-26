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
  // Acquire model data.
  void (*acquire)(void* context, real_t t, real_t* data); 

  // Destructor.
  void (*dtor)(void* context);
} model_probe_vtable;

// Creates an instance of a model probe that acquires a quantity of the given 
// name, placing its data into an array large enough to store all of its 
// contents. Here, datum_size is the number of real numbers required to store 
// a datum.
model_probe_t* model_probe_new(const char* name, 
                               size_t size,
                               void* context, 
                               model_probe_vtable vtable);

// Destroys the probe.
void model_probe_free(model_probe_t* probe);

// Returns the name of the probe.
char* model_probe_name(model_probe_t* probe);

// Returns the size of the probe's datum.
size_t model_probe_datum_size(model_probe_t* probe);

// Acquires the quantity at the given time t, placing it into datum.
void model_probe_acquire(model_probe_t* probe, real_t t, real_t* datum);

#endif
