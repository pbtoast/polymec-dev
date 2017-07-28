// Copyright (c) 2012-2017, Jeffrey N. Johnson
// All rights reserved.
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include "cmocka.h"
#include "core/exchanger.h"

static void test_exchanger_new(void** state)
{
  exchanger_t* exchanger = exchanger_new(MPI_COMM_WORLD);
  exchanger = NULL;
}

static void test_exchanger_construct(void** state)
{
  MPI_Comm comm = MPI_COMM_WORLD;
  int nproc, rank;
  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);
  int N = 100*nproc;
  exchanger_t* exchanger = exchanger_new(comm);
  assert_true(exchanger_comm(exchanger) == comm);
  if (nproc > 1)
  {
    int send_indices[N/nproc];
    for (int i = 0; i < N/nproc; ++i)
      send_indices[i] = i;
    exchanger_set_send(exchanger, (rank+1) % nproc, send_indices, N/nproc, true);
    int receive_indices[N/nproc];
    for (int i = 0; i < N/nproc; ++i)
      send_indices[i] = i;
    exchanger_set_receive(exchanger, (rank+nproc-1) % nproc, receive_indices, N/nproc, true);
  }
  exchanger = NULL;
}

static void test_exchanger_construct_and_delete(void** state)
{
  MPI_Comm comm = MPI_COMM_WORLD;
  int nproc, rank;
  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);
  int N = 100*nproc;
  exchanger_t* exchanger = exchanger_new(comm);
  exchanger_set_send_offset(exchanger, 0);
  exchanger_set_receive_offset(exchanger, 0);
  if (nproc > 1)
  {
    int send_indices[N/nproc];
    for (int i = 0; i < N/nproc; ++i)
      send_indices[i] = i;
    exchanger_set_send(exchanger, (rank+1) % nproc, send_indices, N/nproc, true);
    assert_true(exchanger_max_send(exchanger) != rank);
    int receive_indices[N/nproc];
    for (int i = 0; i < N/nproc; ++i)
      send_indices[i] = i;
    exchanger_set_receive(exchanger, (rank+nproc-1) % nproc, receive_indices, N/nproc, true);
    assert_true(exchanger_max_receive(exchanger) != rank);

    exchanger_delete_send(exchanger, 1);
    exchanger_delete_receive(exchanger, 1);
  }
  exchanger = NULL;
}

int main(int argc, char* argv[]) 
{
  polymec_init(argc, argv);
  const struct CMUnitTest tests[] = 
  {
    cmocka_unit_test(test_exchanger_new),
    cmocka_unit_test(test_exchanger_construct),
    cmocka_unit_test(test_exchanger_construct_and_delete)
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
