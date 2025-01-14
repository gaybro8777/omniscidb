/*
 * Copyright 2020 OmniSci, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstddef>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/locale/generator.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>

#include "QueryEngine/ExtractFromTime.h"
#include "QueryEngine/HyperLogLog.h"
#include "Shared/AuthMetadata.h"
#include "Shared/SystemParameters.h"

namespace po = boost::program_options;

class CommandLineOptions {
 public:
  CommandLineOptions(char const* argv0, bool dist_v5_ = false)
      : log_options_(argv0), dist_v5_(dist_v5_) {
    fillOptions();
    fillAdvancedOptions();
  }
  int http_port = 6278;
  size_t reserved_gpu_mem = 384 * 1024 * 1024;
  std::string base_path;
  std::string cluster_file = {"cluster.conf"};
  std::string cluster_topology_file = {"cluster_topology.conf"};
  std::string license_path = {""};
  std::string encryption_key_store_path = {};
  bool verbose_logging = false;
  bool jit_debug = false;
  bool intel_jit_profile = false;
  bool allow_multifrag = true;
  bool read_only = false;
  bool allow_loop_joins = false;
  bool enable_legacy_syntax = true;
  bool log_user_origin = true;
  AuthMetadata authMetadata;

  SystemParameters system_parameters;
  bool enable_runtime_udf = false;

  bool enable_watchdog = true;
  bool enable_dynamic_watchdog = false;
  bool enable_runtime_query_interrupt = true;
  bool enable_non_kernel_time_query_interrupt = true;
  bool use_estimator_result_cache = true;
  double running_query_interrupt_freq = 0.1;     // 0.0 ~ 1.0
  unsigned pending_query_interrupt_freq = 1000;  // in milliseconds
  unsigned dynamic_watchdog_time_limit = 10000;
  std::string disk_cache_level = "";

  bool enable_data_recycler = true;
  bool use_hashtable_cache = true;
  size_t hashtable_cache_total_bytes = 4294967296;         // 4GB
  size_t max_cacheable_hashtable_size_bytes = 2147483648;  // 2GB

  /**
   * Number of threads used when loading data
   */
  size_t num_reader_threads = 0;
  /**
   * path to file containing warmup queries list
   */
  std::string db_query_file = {""};
  /**
   * exit after warmup
   */
  bool exit_after_warmup = false;
  /**
   * Inactive session tolerance in mins (60 mins)
   */
  int idle_session_duration = kMinsPerHour;
  /**
   * Maximum session life in mins (43,200 mins == 30 Days)
   * (https://pages.nist.gov/800-63-3/sp800-63b.html#aal3reauth)
   */
  int max_session_duration = kMinsPerMonth;
  std::string udf_file_name = {""};
  std::string udf_compiler_path = {""};
  std::vector<std::string> udf_compiler_options;

  std::string allowed_import_paths{};
  std::string allowed_export_paths{};

  void fillOptions();
  void fillAdvancedOptions();

  po::options_description help_desc;
  po::options_description developer_desc;
  logger::LogOptions log_options_;
  po::positional_options_description positional_options;

 public:
  po::variables_map vm;
  std::string clusterIds_arg;

  std::string getNodeIds();
  std::vector<std::string> getNodeIdsArray();

  std::optional<int> parse_command_line(int argc,
                                        char const* const* argv,
                                        const bool should_init_logging = false);
  void validate();
  void validate_base_path();
  void init_logging();
  const bool dist_v5_;
};

extern bool g_enable_table_functions;
extern unsigned g_pending_query_interrupt_freq;

extern size_t g_max_log_length;
