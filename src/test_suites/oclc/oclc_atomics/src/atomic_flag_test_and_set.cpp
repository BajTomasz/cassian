/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <algorithm>
#include <cassian/logging/logging.hpp>
#include <cassian/random/random.hpp>
#include <cassian/runtime/openclc_types.hpp>
#include <cassian/runtime/runtime.hpp>
#include <cassian/test_harness/test_harness.hpp>
#include <cassian/utility/utility.hpp>
#include <catch2/catch.hpp>
#include <common.hpp>
#include <cstddef>
#include <string>
#include <test_config.hpp>
#include <vector>

namespace ca = cassian;

namespace {

template <typename T> struct TestCase {
  using test_type = T;
  using test_host_type = typename T::host_type;

  MemoryType memory_type = MemoryType::global;
  FunctionType function_type = FunctionType::implicit;
  MemoryOrder memory_order = MemoryOrder::relaxed;
  MemoryScope memory_scope = MemoryScope::device;

  int global_work_size = 0;
  int local_work_size = 0;

  ca::Runtime *runtime = nullptr;
  std::string program_type;

  std::vector<test_host_type> input;
};

template <typename T> std::vector<T> generate_input(const int size) {
  return std::vector<T>(size, false);
}

std::string get_kernel_path(const MemoryType memory_type) {
  switch (memory_type) {
  case MemoryType::global:
    return "kernels/oclc_atomics/atomic_flag_test_and_set_global.cl";
  case MemoryType::local:
    return "kernels/oclc_atomics/atomic_flag_test_and_set_local.cl";
  default:
    return "unknown";
  }
}

template <typename TEST_TYPE>
std::string get_build_options(const int local_work_size,
                              const FunctionType function_type,
                              const MemoryOrder memory_order,
                              const MemoryScope memory_scope) {
  std::string build_options = " -cl-std=CL3.0" +
                              data_type_build_option<TEST_TYPE>() +
                              work_group_size_build_option(local_work_size);

  if (function_type == FunctionType::explicit_memory_order) {
    build_options += memory_order_build_option(memory_order);
  } else if (function_type == FunctionType::explicit_memory_scope) {
    build_options += memory_order_build_option(memory_order) +
                     memory_scope_build_option(memory_scope);
  }

  return build_options;
}

template <typename T> struct Output {
  std::vector<T> result;
  std::vector<T> fetched;
};

template <typename T>
Output<T> run_kernel(const ca::Kernel &kernel, const int global_work_size,
                     const int local_work_size, const std::vector<T> &input,
                     ca::Runtime *runtime) {
  std::vector<ca::Buffer> buffers;

  ca::Buffer value_buffer = runtime->create_buffer(sizeof(T) * input.size());
  runtime->write_buffer_from_vector(value_buffer, input);
  buffers.push_back(value_buffer);

  ca::Buffer fetched_buffer = runtime->create_buffer(sizeof(T) * input.size());
  buffers.push_back(fetched_buffer);

  for (size_t i = 0; i < buffers.size(); ++i) {
    runtime->set_kernel_argument(kernel, static_cast<int>(i), buffers[i]);
  }

  runtime->run_kernel(kernel, global_work_size, local_work_size);

  Output<T> output;
  output.result = runtime->read_buffer_to_vector<T>(value_buffer);
  output.fetched = runtime->read_buffer_to_vector<T>(fetched_buffer);

  for (auto &buffer : buffers) {
    runtime->release_buffer(buffer);
  }

  return output;
}

template <typename TEST_CASE_TYPE> void run_test(TEST_CASE_TYPE test_case) {
  using test_type = typename TEST_CASE_TYPE::test_type;
  using test_host_type = typename TEST_CASE_TYPE::test_host_type;

  const std::string kernel_path = get_kernel_path(test_case.memory_type);
  const std::string build_options = get_build_options<test_type>(
      test_case.local_work_size, test_case.function_type,
      test_case.memory_order, test_case.memory_scope);
  const ca::Kernel kernel = create_kernel(
      kernel_path, build_options, test_case.runtime, test_case.program_type);

  std::vector<test_host_type> expected(test_case.local_work_size, true);
  const Output<test_host_type> output =
      run_kernel(kernel, test_case.global_work_size, test_case.local_work_size,
                 test_case.input, test_case.runtime);
  test_case.runtime->release_kernel(kernel);

  REQUIRE_THAT(output.fetched, Catch::Equals(test_case.input));
  REQUIRE_THAT(output.result, Catch::Equals(expected));
}

template <typename TEST_CASE_TYPE>
void test_signatures(TEST_CASE_TYPE test_case,
                     const std::vector<MemoryType> &memory_types,
                     const std::vector<FunctionType> &function_types,
                     const std::vector<MemoryOrder> &memory_orders,
                     const std::vector<MemoryScope> &memory_scopes) {
  for (const auto memory_type : memory_types) {
    test_case.memory_type = memory_type;
    SECTION(to_string(memory_type)) {
      for (const auto function_type : function_types) {
        test_case.function_type = function_type;
        SECTION(to_string(function_type)) {
          if (!is_function_type_supported(
                  test_case.runtime, test_case.program_type, function_type)) {
            ca::logging::info()
                << to_string(function_type) << " section skipped\n";
            continue;
          }
          if (function_type == FunctionType::implicit) {
            run_test(test_case);
          } else if (function_type == FunctionType::explicit_memory_order) {
            for (const auto memory_order : memory_orders) {
              test_case.memory_order = memory_order;
              SECTION(to_string(memory_order)) {
                if (!is_memory_order_supported(test_case.runtime,
                                               test_case.program_type,
                                               memory_order)) {
                  ca::logging::info()
                      << to_string(memory_order) << " section skipped\n";
                  continue;
                }
                run_test(test_case);
              }
            }
          } else if (function_type == FunctionType::explicit_memory_scope) {
            for (const auto memory_order : memory_orders) {
              SECTION(to_string(memory_order)) {
                test_case.memory_order = memory_order;
                if (!is_memory_order_supported(test_case.runtime,
                                               test_case.program_type,
                                               memory_order)) {
                  ca::logging::info()
                      << to_string(memory_order) << " section skipped\n";
                  continue;
                }
                for (const auto memory_scope : memory_scopes) {
                  test_case.memory_scope = memory_scope;
                  SECTION(to_string(memory_scope)) {
                    if (!is_memory_scope_supported(test_case.runtime,
                                                   test_case.program_type,
                                                   memory_scope)) {
                      ca::logging::info()
                          << to_string(memory_scope) << " section skipped\n";
                      continue;
                    }
                    run_test(test_case);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST_CASE("atomic_flag_test_and_set_signatures", "") {
  const TestConfig &config = get_test_config();

  using test_case_type = TestCase<ca::clc_int_t>;
  auto test_case = create_test_case<test_case_type>(config);

  test_case.input = generate_input<typename test_case_type::test_host_type>(
      test_case.global_work_size);
  test_signatures(test_case, memory_types_all, function_types_all,
                  memory_orders_all,
                  {MemoryScope::work_group, MemoryScope::device,
                   MemoryScope::all_svm_devices});
}

} // namespace