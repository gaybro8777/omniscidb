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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <optional>

#include "CommandLineOptions.h"
#include "MapDRelease.h"
#include "OSDependent/omnisci_fs.h"
#include "StringDictionary/StringDictionary.h"

#ifdef _WIN32
#include <io.h>
#endif

bool g_use_table_device_offset;  // TODO(adb): where did this go?
extern bool g_cache_string_hash;
extern int64_t g_large_ndv_threshold;
extern size_t g_large_ndv_multiplier;
extern int64_t g_bitmap_memory_limit;
extern size_t g_approx_quantile_buffer;
extern size_t g_approx_quantile_centroids;
extern size_t g_estimator_failure_max_groupby_size;
extern bool g_columnar_large_projections;
extern size_t g_columnar_large_projections_threshold;

unsigned connect_timeout{20000};
unsigned recv_timeout{300000};
unsigned send_timeout{300000};
bool with_keepalive{false};

void CommandLineOptions::init_logging() {
  if (verbose_logging && logger::Severity::DEBUG1 < log_options_.severity_) {
    log_options_.severity_ = logger::Severity::DEBUG1;
  }
  validate_base_path();
  log_options_.set_base_path(base_path);
  logger::init(log_options_);
}

void CommandLineOptions::fillOptions() {
  help_desc.add_options()("help,h", "Show available options.");
  help_desc.add_options()("allow-loop-joins",
                          po::value<bool>(&allow_loop_joins)
                              ->default_value(allow_loop_joins)
                              ->implicit_value(true),
                          "Enable loop joins.");
  help_desc.add_options()("calcite-max-mem",
                          po::value<size_t>(&system_parameters.calcite_max_mem)
                              ->default_value(system_parameters.calcite_max_mem),
                          "Max memory available to calcite JVM.");
  if (!dist_v5_) {
    help_desc.add_options()("calcite-port",
                            po::value<int>(&system_parameters.calcite_port)
                                ->default_value(system_parameters.calcite_port),
                            "Calcite port number.");
  }
  help_desc.add_options()("config",
                          po::value<std::string>(&system_parameters.config_file),
                          "Path to server configuration file.");
  help_desc.add_options()("cpu-buffer-mem-bytes",
                          po::value<size_t>(&system_parameters.cpu_buffer_mem_bytes)
                              ->default_value(system_parameters.cpu_buffer_mem_bytes),
                          "Size of memory reserved for CPU buffers, in bytes.");

  help_desc.add_options()("cpu-only",
                          po::value<bool>(&system_parameters.cpu_only)
                              ->default_value(system_parameters.cpu_only)
                              ->implicit_value(true),
                          "Run on CPU only, even if GPUs are available.");
  help_desc.add_options()("cuda-block-size",
                          po::value<size_t>(&system_parameters.cuda_block_size)
                              ->default_value(system_parameters.cuda_block_size),
                          "Size of block to use on GPU.");
  help_desc.add_options()("cuda-grid-size",
                          po::value<size_t>(&system_parameters.cuda_grid_size)
                              ->default_value(system_parameters.cuda_grid_size),
                          "Size of grid to use on GPU.");
  if (!dist_v5_) {
    help_desc.add_options()(
        "data",
        po::value<std::string>(&base_path)->required()->default_value("data"),
        "Directory path to OmniSci data storage (catalogs, raw data, log files, etc).");
    positional_options.add("data", 1);
  }
  help_desc.add_options()("db-query-list",
                          po::value<std::string>(&db_query_file),
                          "Path to file containing OmniSci warmup queries.");
  help_desc.add_options()(
      "exit-after-warmup",
      po::value<bool>(&exit_after_warmup)->default_value(false)->implicit_value(true),
      "Exit after OmniSci warmup queries.");
  help_desc.add_options()("enable-debug-timer",
                          po::value<bool>(&g_enable_debug_timer)
                              ->default_value(g_enable_debug_timer)
                              ->implicit_value(true),
                          "Enable debug timer logging.");
  help_desc.add_options()("enable-dynamic-watchdog",
                          po::value<bool>(&enable_dynamic_watchdog)
                              ->default_value(enable_dynamic_watchdog)
                              ->implicit_value(true),
                          "Enable dynamic watchdog.");
  help_desc.add_options()("pending-query-interrupt-freq",
                          po::value<unsigned>(&pending_query_interrupt_freq)
                              ->default_value(pending_query_interrupt_freq)
                              ->implicit_value(1000),
                          "A frequency of checking the request of pending query "
                          "interrupt from user (in millisecond).");

  if (!dist_v5_) {
    help_desc.add_options()(
        "enable-string-dict-hash-cache",
        po::value<bool>(&g_cache_string_hash)
            ->default_value(g_cache_string_hash)
            ->implicit_value(true),
        "Cache string hash values in the string dictionary server during import.");
  }
  help_desc.add_options()("enable-watchdog",
                          po::value<bool>(&enable_watchdog)
                              ->default_value(enable_watchdog)
                              ->implicit_value(true),
                          "Enable watchdog.");

  help_desc.add_options()("gpu-buffer-mem-bytes",
                          po::value<size_t>(&system_parameters.gpu_buffer_mem_bytes)
                              ->default_value(system_parameters.gpu_buffer_mem_bytes),
                          "Size of memory reserved for GPU buffers, in bytes, per GPU.");
  help_desc.add_options()("gpu-input-mem-limit",
                          po::value<double>(&system_parameters.gpu_input_mem_limit)
                              ->default_value(system_parameters.gpu_input_mem_limit),
                          "Force query to CPU when input data memory usage exceeds this "
                          "percentage of available GPU memory.");
  if (!dist_v5_) {
    help_desc.add_options()("http-port",
                            po::value<int>(&http_port)->default_value(http_port),
                            "HTTP port number.");
  }
  help_desc.add_options()(
      "idle-session-duration",
      po::value<int>(&idle_session_duration)->default_value(idle_session_duration),
      "Maximum duration of idle session.");
  help_desc.add_options()(
      "max-session-duration",
      po::value<int>(&max_session_duration)->default_value(max_session_duration),
      "Maximum duration of active session.");
  help_desc.add_options()("num-sessions",
                          po::value<int>(&system_parameters.num_sessions)
                              ->default_value(system_parameters.num_sessions),
                          "Maximum number of active session.");
  help_desc.add_options()(
      "num-reader-threads",
      po::value<size_t>(&num_reader_threads)->default_value(num_reader_threads),
      "Number of reader threads to use.");
  if (!dist_v5_) {
    help_desc.add_options()("port,p",
                            po::value<int>(&system_parameters.omnisci_server_port)
                                ->default_value(system_parameters.omnisci_server_port),
                            "TCP Port number.");
  }
  help_desc.add_options()("num-gpus",
                          po::value<int>(&system_parameters.num_gpus)
                              ->default_value(system_parameters.num_gpus),
                          "Number of gpus to use.");
  help_desc.add_options()(
      "read-only",
      po::value<bool>(&read_only)->default_value(read_only)->implicit_value(true),
      "Enable read-only mode.");

  help_desc.add_options()(
      "res-gpu-mem",
      po::value<size_t>(&reserved_gpu_mem)->default_value(reserved_gpu_mem),
      "Reduces GPU memory available to the OmniSci allocator by this amount. Used for "
      "compiled code cache and ancillary GPU functions and other processes that may also "
      "be using the GPU concurrent with OmniSciDB.");

  help_desc.add_options()("start-gpu",
                          po::value<int>(&system_parameters.start_gpu)
                              ->default_value(system_parameters.start_gpu),
                          "First gpu to use.");
  help_desc.add_options()("verbose",
                          po::value<bool>(&verbose_logging)
                              ->default_value(verbose_logging)
                              ->implicit_value(true),
                          "Write additional debug log messages to server logs.");
  help_desc.add_options()(
      "enable-runtime-udf",
      po::value<bool>(&enable_runtime_udf)
          ->default_value(enable_runtime_udf)
          ->implicit_value(true),
      "Enable runtime UDF registration by passing signatures and corresponding LLVM IR "
      "to the `register_runtime_udf` endpoint. For use with the Python Remote Backend "
      "Compiler server, packaged separately.");
  help_desc.add_options()("version,v", "Print Version Number.");

  help_desc.add_options()(
      "calcite-service-timeout",
      po::value<size_t>(&system_parameters.calcite_timeout)
          ->default_value(system_parameters.calcite_timeout),
      "Calcite server timeout (milliseconds). Increase this on systems with frequent "
      "schema changes or when running large numbers of parallel queries.");
  help_desc.add_options()("calcite-service-keepalive",
                          po::value<size_t>(&system_parameters.calcite_keepalive)
                              ->default_value(system_parameters.calcite_keepalive)
                              ->implicit_value(true),
                          "Enable keepalive on Calcite connections.");
  help_desc.add_options()(
      "stringdict-parallelizm",
      po::value<bool>(&g_enable_stringdict_parallel)
          ->default_value(g_enable_stringdict_parallel)
          ->implicit_value(true),
      "Allow StringDictionary to parallelize loads using multiple threads");
  help_desc.add_options()("log-user-origin",
                          po::value<bool>(&log_user_origin)
                              ->default_value(log_user_origin)
                              ->implicit_value(true),
                          "Lookup the origin of inbound connections by IP address/DNS "
                          "name, and print this information as part of stdlog.");
  help_desc.add_options()(
      "allowed-import-paths",
      po::value<std::string>(&allowed_import_paths),
      "List of allowed root paths that can be used in import operations.");
  help_desc.add_options()(
      "allowed-export-paths",
      po::value<std::string>(&allowed_export_paths),
      "List of allowed root paths that can be used in export operations.");

  help_desc.add(log_options_.get_options());
}

void CommandLineOptions::fillAdvancedOptions() {
  developer_desc.add_options()("dev-options", "Print internal developer options.");
  developer_desc.add_options()(
      "enable-calcite-view-optimize",
      po::value<bool>(&system_parameters.enable_calcite_view_optimize)
          ->default_value(system_parameters.enable_calcite_view_optimize)
          ->implicit_value(true),
      "Enable additional calcite (query plan) optimizations when a view is part of the "
      "query.");
  developer_desc.add_options()("enable-legacy-syntax",
                               po::value<bool>(&enable_legacy_syntax)
                                   ->default_value(enable_legacy_syntax)
                                   ->implicit_value(true),
                               "Enable legacy syntax.");
  developer_desc.add_options()(
      "enable-multifrag",
      po::value<bool>(&allow_multifrag)
          ->default_value(allow_multifrag)
          ->implicit_value(true),
      "Enable execution over multiple fragments in a single round-trip to GPU.");
  developer_desc.add_options()("num-executors",
                               po::value<int>(&system_parameters.num_executors)
                                   ->default_value(system_parameters.num_executors),
                               "Number of executors to run in parallel.");
  developer_desc.add_options()(
      "offset-device-by-table-id",
      po::value<bool>(&g_use_table_device_offset)
          ->default_value(g_use_table_device_offset)
          ->implicit_value(true),
      "Enables/disables offseting the chosen device ID by the table ID for a given "
      "fragment. This improves balance of fragments across GPUs.");
  developer_desc.add_options()("enable-table-functions",
                               po::value<bool>(&g_enable_table_functions)
                                   ->default_value(g_enable_table_functions)
                                   ->implicit_value(true),
                               "Enable experimental table functions support.");
  developer_desc.add_options()(
      "jit-debug-ir",
      po::value<bool>(&jit_debug)->default_value(jit_debug)->implicit_value(true),
      "Enable runtime debugger support for the JIT. Note that this flag is "
      "incompatible "
      "with the `ENABLE_JIT_DEBUG` build flag. The generated code can be found at "
      "`/tmp/mapdquery`.");
  developer_desc.add_options()(
      "intel-jit-profile",
      po::value<bool>(&intel_jit_profile)
          ->default_value(intel_jit_profile)
          ->implicit_value(true),
      "Enable runtime support for the JIT code profiling using Intel VTune.");

  developer_desc.add_options()(
      "min-cpu-slab-size",
      po::value<size_t>(&system_parameters.min_cpu_slab_size)
          ->default_value(system_parameters.min_cpu_slab_size),
      "Min slab size (size of memory allocations) for CPU buffer pool.");
  developer_desc.add_options()(
      "max-cpu-slab-size",
      po::value<size_t>(&system_parameters.max_cpu_slab_size)
          ->default_value(system_parameters.max_cpu_slab_size),
      "Max CPU buffer pool slab size (size of memory allocations). Note if "
      "there is not enough free memory to accomodate the target slab size, smaller "
      "slabs will be allocated, down to the minimum size specified by "
      "min-cpu-slab-size.");
  developer_desc.add_options()(
      "min-gpu-slab-size",
      po::value<size_t>(&system_parameters.min_gpu_slab_size)
          ->default_value(system_parameters.min_gpu_slab_size),
      "Min slab size (size of memory allocations) for GPU buffer pools.");
  developer_desc.add_options()(
      "max-gpu-slab-size",
      po::value<size_t>(&system_parameters.max_gpu_slab_size)
          ->default_value(system_parameters.max_gpu_slab_size),
      "Max GPU buffer pool slab size (size of memory allocations). Note if "
      "there is not enough free memory to accomodate the target slab size, smaller "
      "slabs will be allocated, down to the minimum size speified by "
      "min-gpu-slab-size.");

  developer_desc.add_options()("ssl-cert",
                               po::value<std::string>(&system_parameters.ssl_cert_file)
                                   ->default_value(std::string("")),
                               "SSL Validated public certficate.");

  developer_desc.add_options()("ssl-private-key",
                               po::value<std::string>(&system_parameters.ssl_key_file)
                                   ->default_value(std::string("")),
                               "SSL private key file.");
  // Note ssl_trust_store is passed through to Calcite via system_parameters
  // todo(jack): add ensure ssl-trust-store exists if cert and private key in use
  developer_desc.add_options()("ssl-trust-store",
                               po::value<std::string>(&system_parameters.ssl_trust_store)
                                   ->default_value(std::string("")),
                               "SSL public CA certifcates (java trust store) to validate "
                               "TLS connections (passed through to the Calcite server).");

  developer_desc.add_options()(
      "ssl-trust-password",
      po::value<std::string>(&system_parameters.ssl_trust_password)
          ->default_value(std::string("")),
      "SSL password for java trust store provided via --ssl-trust-store parameter.");

  developer_desc.add_options()(
      "ssl-trust-ca",
      po::value<std::string>(&system_parameters.ssl_trust_ca_file)
          ->default_value(std::string("")),
      "SSL public CA certificates to validate TLS connection(as a client).");

  developer_desc.add_options()(
      "ssl-trust-ca-server",
      po::value<std::string>(&authMetadata.ca_file_name)->default_value(std::string("")),
      "SSL public CA certificates to validate TLS connection(as a server).");

  developer_desc.add_options()("ssl-keystore",
                               po::value<std::string>(&system_parameters.ssl_keystore)
                                   ->default_value(std::string("")),
                               "SSL server credentials as a java key store (passed "
                               "through to the Calcite server).");

  developer_desc.add_options()(
      "ssl-keystore-password",
      po::value<std::string>(&system_parameters.ssl_keystore_password)
          ->default_value(std::string("")),
      "SSL password for java keystore, provide by via --ssl-keystore.");

  developer_desc.add_options()(
      "udf",
      po::value<std::string>(&udf_file_name),
      "Load user defined extension functions from this file at startup. The file is "
      "expected to be a C/C++ file with extension .cpp.");

  developer_desc.add_options()(
      "udf-compiler-path",
      po::value<std::string>(&udf_compiler_path),
      "Provide absolute path to clang++ used in udf compilation.");

  developer_desc.add_options()("udf-compiler-options",
                               po::value<std::vector<std::string>>(&udf_compiler_options),
                               "Specify compiler options to tailor udf compilation.");

  developer_desc.add_options()(
      "large-ndv-threshold",
      po::value<int64_t>(&g_large_ndv_threshold)->default_value(g_large_ndv_threshold));
  developer_desc.add_options()(
      "large-ndv-multiplier",
      po::value<size_t>(&g_large_ndv_multiplier)->default_value(g_large_ndv_multiplier));
  developer_desc.add_options()("approx_quantile_buffer",
                               po::value<size_t>(&g_approx_quantile_buffer)
                                   ->default_value(g_approx_quantile_buffer));
  developer_desc.add_options()("approx_quantile_centroids",
                               po::value<size_t>(&g_approx_quantile_centroids)
                                   ->default_value(g_approx_quantile_centroids));
  developer_desc.add_options()(
      "bitmap-memory-limit",
      po::value<int64_t>(&g_bitmap_memory_limit)->default_value(g_bitmap_memory_limit),
      "Limit for count distinct bitmap memory use. The limit is computed by taking the "
      "size of the group by buffer (entry count in Query Memory Descriptor) and "
      "multiplying it by the number of count distinct expression and the size of bitmap "
      "required for each. For approx_count_distinct this is typically 8192 bytes.");
  developer_desc.add_options()(
      "max-log-length",
      po::value<size_t>(&g_max_log_length)->default_value(g_max_log_length),
      "The maximum number of characters that a log message can has. If the log message "
      "is longer than this, we only record \'g_max_log_message_length\' characters.");
  developer_desc.add_options()(
      "estimator-failure-max-groupby-size",
      po::value<size_t>(&g_estimator_failure_max_groupby_size)
          ->default_value(g_estimator_failure_max_groupby_size),
      "Maximum size of the groupby buffer if the estimator fails. By default we use the "
      "number of tuples in the table up to this value.");
  developer_desc.add_options()("columnar-large-projections",
                               po::value<bool>(&g_columnar_large_projections)
                                   ->default_value(g_columnar_large_projections)
                                   ->implicit_value(true),
                               "Prefer columnar output if projection size is >= "
                               "threshold set by --columnar-large-projections-threshold "
                               "(default 1,000,000 rows).");
  developer_desc.add_options()(
      "columnar-large-projections-threshold",
      po::value<size_t>(&g_columnar_large_projections_threshold)
          ->default_value(g_columnar_large_projections_threshold),
      "Threshold (in minimum number of rows) to prefer columnar output for projections. "
      "Requires --columnar-large-projections to be set.");
}

namespace {

std::stringstream sanitize_config_file(std::ifstream& in) {
  // Strip the web section out of the config file so boost can validate program options
  std::stringstream ss;
  std::string line;
  while (std::getline(in, line)) {
    ss << line << "\n";
    if (line == "[web]") {
      break;
    }
  }
  return ss;
}

bool trim_and_check_file_exists(std::string& filename, const std::string desc) {
  if (!filename.empty()) {
    boost::algorithm::trim_if(filename, boost::is_any_of("\"'"));
    if (!boost::filesystem::exists(filename)) {
      std::cerr << desc << " " << filename << " does not exist." << std::endl;
      return false;
    }
  }
  return true;
}

}  // namespace

void CommandLineOptions::validate_base_path() {
  boost::algorithm::trim_if(base_path, boost::is_any_of("\"'"));
  if (!boost::filesystem::exists(base_path)) {
    throw std::runtime_error("OmniSci base directory does not exist at " + base_path);
  }
}

void CommandLineOptions::validate() {
  boost::algorithm::trim_if(base_path, boost::is_any_of("\"'"));
  const auto data_path = boost::filesystem::path(base_path) / "mapd_data";
  if (!boost::filesystem::exists(data_path)) {
    throw std::runtime_error("OmniSci data directory does not exist at '" + base_path +
                             "'");
  }

  {
    const auto lock_file = boost::filesystem::path(base_path) / "omnisci_server_pid.lck";
    auto pid = std::to_string(getpid());

    int pid_fd = omnisci::open(lock_file.string().c_str(), O_RDWR | O_CREAT, 0644);
    if (pid_fd == -1) {
      auto err = std::string("Failed to open PID file ") + lock_file.string().c_str() +
                 ". " + strerror(errno) + ".";
      throw std::runtime_error(err);
    }
// TODO: support lock on Windows
#ifndef _WIN32
    if (lockf(pid_fd, F_TLOCK, 0) == -1) {
      omnisci::close(pid_fd);
      auto err = std::string("Another OmniSci Server is using data directory ") +
                 base_path + ".";
      throw std::runtime_error(err);
    }
#endif
    if (omnisci::ftruncate(pid_fd, 0) == -1) {
      omnisci::close(pid_fd);
      auto err = std::string("Failed to truncate PID file ") +
                 lock_file.string().c_str() + ". " + strerror(errno) + ".";
      throw std::runtime_error(err);
    }
    if (write(pid_fd, pid.c_str(), pid.length()) == -1) {
      omnisci::close(pid_fd);
      auto err = std::string("Failed to write PID file ") + lock_file.string().c_str() +
                 ". " + strerror(errno) + ".";
      throw std::runtime_error(err);
    }
  }
  boost::algorithm::trim_if(db_query_file, boost::is_any_of("\"'"));
  if (db_query_file.length() > 0 && !boost::filesystem::exists(db_query_file)) {
    throw std::runtime_error("File containing DB queries " + db_query_file +
                             " does not exist.");
  }
  if (license_path.length() == 0) {
    license_path = base_path + "/omnisci.license";
  }

  // add all parameters to be displayed on startup
  LOG(INFO) << "OmniSci started with data directory at '" << base_path << "'";
  if (vm.count("license-path")) {
    LOG(INFO) << "License key path set to '" << license_path << "'";
  }
  LOG(INFO) << " Server read-only mode is " << read_only;
#if DISABLE_CONCURRENCY
  LOG(INFO) << " Threading layer: serial";
#elif ENABLE_TBB
  LOG(INFO) << " Threading layer: TBB";
#else
  LOG(INFO) << " Threading layer: std";
#endif
  LOG(INFO) << " Watchdog is set to " << enable_watchdog;
  LOG(INFO) << " Dynamic Watchdog is set to " << enable_dynamic_watchdog;
  if (enable_dynamic_watchdog) {
    LOG(INFO) << " Dynamic Watchdog timeout is set to " << dynamic_watchdog_time_limit;
  }
  LOG(INFO) << " Runtime query interrupt is set to " << enable_runtime_query_interrupt;
  if (enable_runtime_query_interrupt) {
    LOG(INFO) << " A frequency of checking pending query interrupt request is set to "
              << pending_query_interrupt_freq << " (in ms.)";
    LOG(INFO) << " A frequency of checking running query interrupt request is set to "
              << running_query_interrupt_freq << " (0.0 ~ 1.0)";
  }
  LOG(INFO) << " Non-kernel time query interrupt is set to "
            << enable_non_kernel_time_query_interrupt;

  LOG(INFO) << " Debug Timer is set to " << g_enable_debug_timer;
  LOG(INFO) << " Maximum idle session duration " << idle_session_duration;
  LOG(INFO) << " Maximum active session duration " << max_session_duration;
  LOG(INFO) << " Maximum number of sessions " << system_parameters.num_sessions;

  LOG(INFO) << "Allowed import paths is set to " << allowed_import_paths;
  LOG(INFO) << "Allowed export paths is set to " << allowed_export_paths;
}

std::optional<int> CommandLineOptions::parse_command_line(
    int argc,
    char const* const* argv,
    const bool should_init_logging) {
  po::options_description all_desc("All options");
  all_desc.add(help_desc).add(developer_desc);

  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(all_desc)
                  .positional(positional_options)
                  .run(),
              vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cerr << "Usage: omnisci_server <data directory path> [-p <port number>] "
                   "[--http-port <http port number>] [--flush-log] [--version|-v]"
                << std::endl
                << std::endl;
      std::cout << help_desc << std::endl;
      return 0;
    }
    if (vm.count("dev-options")) {
      std::cout << "Usage: omnisci_server <data directory path> [-p <port number>] "
                   "[--http-port <http port number>] [--flush-log] [--version|-v]"
                << std::endl
                << std::endl;
      std::cout << developer_desc << std::endl;
      return 0;
    }
    if (vm.count("version")) {
      std::cout << "OmniSci Version: " << MAPD_RELEASE << std::endl;
      return 0;
    }

    if (vm.count("config")) {
      std::ifstream settings_file(system_parameters.config_file);

      auto sanitized_settings = sanitize_config_file(settings_file);

      po::store(po::parse_config_file(sanitized_settings, all_desc, false), vm);
      po::notify(vm);
      settings_file.close();
    }

    if (should_init_logging) {
      init_logging();
    }

    if (!trim_and_check_file_exists(system_parameters.ssl_cert_file, "ssl cert file")) {
      return 1;
    }
    if (!trim_and_check_file_exists(authMetadata.ca_file_name, "ca file name")) {
      return 1;
    }
    if (!trim_and_check_file_exists(system_parameters.ssl_trust_store,
                                    "ssl trust store")) {
      return 1;
    }
    if (!trim_and_check_file_exists(system_parameters.ssl_keystore, "ssl key store")) {
      return 1;
    }
    if (!trim_and_check_file_exists(system_parameters.ssl_key_file, "ssl key file")) {
      return 1;
    }
    if (!trim_and_check_file_exists(system_parameters.ssl_trust_ca_file, "ssl ca file")) {
      return 1;
    }

    g_pending_query_interrupt_freq = pending_query_interrupt_freq;

  } catch (po::error& e) {
    std::cerr << "Usage Error: " << e.what() << std::endl;
    return 1;
  }

  if (vm.count("udf")) {
    boost::algorithm::trim_if(udf_file_name, boost::is_any_of("\"'"));

    if (!boost::filesystem::exists(udf_file_name)) {
      LOG(ERROR) << " User defined function file " << udf_file_name << " does not exist.";
      return 1;
    }

    LOG(INFO) << " User provided extension functions loaded from " << udf_file_name;
  }

  if (vm.count("udf-compiler-path")) {
    boost::algorithm::trim_if(udf_compiler_path, boost::is_any_of("\"'"));
  }

  auto trim_string = [](std::string& s) {
    boost::algorithm::trim_if(s, boost::is_any_of("\"'"));
  };

  if (vm.count("udf-compiler-options")) {
    std::for_each(udf_compiler_options.begin(), udf_compiler_options.end(), trim_string);
  }

  if (enable_runtime_udf) {
    LOG(INFO) << " Runtime user defined extension functions enabled globally.";
  }

  boost::algorithm::trim_if(system_parameters.ha_brokers, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(system_parameters.ha_group_id, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(system_parameters.ha_shared_data, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(system_parameters.ha_unique_server_id,
                            boost::is_any_of("\"'"));

  if (!system_parameters.ha_group_id.empty()) {
    LOG(INFO) << " HA group id " << system_parameters.ha_group_id;
    if (system_parameters.ha_unique_server_id.empty()) {
      LOG(ERROR) << "Starting server in HA mode --ha-unique-server-id must be set ";
      return 5;
    } else {
      LOG(INFO) << " HA unique server id " << system_parameters.ha_unique_server_id;
    }
    if (system_parameters.ha_brokers.empty()) {
      LOG(ERROR) << "Starting server in HA mode --ha-brokers must be set ";
      return 6;
    } else {
      LOG(INFO) << " HA brokers " << system_parameters.ha_brokers;
    }
    if (system_parameters.ha_shared_data.empty()) {
      LOG(ERROR) << "Starting server in HA mode --ha-shared-data must be set ";
      return 7;
    } else {
      LOG(INFO) << " HA shared data is " << system_parameters.ha_shared_data;
    }
  }

  boost::algorithm::trim_if(system_parameters.master_address, boost::is_any_of("\"'"));
  if (!system_parameters.master_address.empty()) {
    if (!read_only) {
      LOG(ERROR) << "The master-address setting is only allowed in read-only mode";
      return 9;
    }
    LOG(INFO) << " Master Address is " << system_parameters.master_address;
    LOG(INFO) << " Master Port is " << system_parameters.master_port;
  }

  LOG(INFO) << " cuda block size " << system_parameters.cuda_block_size;
  LOG(INFO) << " cuda grid size  " << system_parameters.cuda_grid_size;
  LOG(INFO) << " Min CPU buffer pool slab size " << system_parameters.min_cpu_slab_size;
  LOG(INFO) << " Max CPU buffer pool slab size " << system_parameters.max_cpu_slab_size;
  LOG(INFO) << " Min GPU buffer pool slab size " << system_parameters.min_gpu_slab_size;
  LOG(INFO) << " Max GPU buffer pool slab size " << system_parameters.max_gpu_slab_size;
  LOG(INFO) << " calcite JVM max memory  " << system_parameters.calcite_max_mem;
  LOG(INFO) << " OmniSci Server Port  " << system_parameters.omnisci_server_port;
  LOG(INFO) << " OmniSci Calcite Port  " << system_parameters.calcite_port;
  LOG(INFO) << " Enable Calcite view optimize "
            << system_parameters.enable_calcite_view_optimize;
  LOG(INFO) << " Allow Local Auth Fallback: "
            << (authMetadata.allowLocalAuthFallback ? "enabled" : "disabled");

  boost::algorithm::trim_if(authMetadata.distinguishedName, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(authMetadata.uri, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(authMetadata.ldapQueryUrl, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(authMetadata.ldapRoleRegex, boost::is_any_of("\"'"));
  boost::algorithm::trim_if(authMetadata.ldapSuperUserRole, boost::is_any_of("\"'"));

  return std::nullopt;
}