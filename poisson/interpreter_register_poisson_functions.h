#ifndef POLYMEC_INTERPRETER_REGISTER_POISSON_FUNCTIONS_H
#define POLYMEC_INTERPRETER_REGISTER_POISSON_FUNCTIONS_H

#include "core/interpreter.h"
#include "poisson/poisson_bc.h"

// Equips the given interpreter with functions specific to the Poisson model.
void interpreter_register_poisson_functions(interpreter_t* interpreter);

#endif
