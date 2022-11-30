/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cassian/cli/cli.hpp>
#include <cassian/runtime/factory.hpp>
#include <cassian/runtime/runtime.hpp>
#include <memory>
#include <string>
#include <test_config.hpp>

namespace ca = cassian;

TestConfig::TestConfig(const ca::CommandLineParser &parser)
    : TestConfigBase(parser) {}

const TestConfig *config = nullptr;
const TestConfig &get_test_config() { return *config; }
void set_test_config(const TestConfig &c) { config = &c; }
