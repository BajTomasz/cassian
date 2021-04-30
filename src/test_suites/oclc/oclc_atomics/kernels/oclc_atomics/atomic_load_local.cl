/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// TODO: Move it to the host
#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__attribute__((reqd_work_group_size(WORK_GROUP_SIZE, 1, 1))) kernel void
test_kernel(const global DATA_TYPE *input, global DATA_TYPE *output) {
  const size_t global_id = get_global_id(0);
  const size_t local_id = get_local_id(0);

  local ATOMIC_TYPE local_atomic[WORK_GROUP_SIZE];
  atomic_store(&local_atomic[local_id], input[global_id]);

#if defined(MEMORY_SCOPE) && defined(MEMORY_ORDER)
  output[global_id] =
      atomic_load_explicit(&local_atomic[local_id], MEMORY_ORDER, MEMORY_SCOPE);
#elif defined(MEMORY_ORDER)
  output[global_id] =
      atomic_load_explicit(&local_atomic[local_id], MEMORY_ORDER);
#else
  output[global_id] = atomic_load(&local_atomic[local_id]);
#endif
}
