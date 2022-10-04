/*
 * Copyright 2018 OmniSci, Inc.
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

#include "DateAdd.h"
#include "DateTruncate.h"

#include "IR/Type.h"

#include <cstdint>
#include <ctime>
#include <map>
#include <string>

#include "../Shared/sqldefs.h"

namespace {

static const std::map<std::pair<int32_t, ExtractField>, std::pair<SQLOps, int64_t>>
    orig_extract_precision_lookup = {{{3, kMICROSECOND}, {kMULTIPLY, kMilliSecsPerSec}},
                                     {{3, kNANOSECOND}, {kMULTIPLY, kMicroSecsPerSec}},
                                     {{6, kMILLISECOND}, {kDIVIDE, kMilliSecsPerSec}},
                                     {{6, kNANOSECOND}, {kMULTIPLY, kMilliSecsPerSec}},
                                     {{9, kMILLISECOND}, {kDIVIDE, kMicroSecsPerSec}},
                                     {{9, kMICROSECOND}, {kDIVIDE, kMilliSecsPerSec}}};

static const std::map<std::pair<hdk::ir::TimeUnit, ExtractField>,
                      std::pair<SQLOps, int64_t>>
    extract_precision_lookup = {
        {{hdk::ir::TimeUnit::kMilli, kMICROSECOND}, {kMULTIPLY, kMilliSecsPerSec}},
        {{hdk::ir::TimeUnit::kMilli, kNANOSECOND}, {kMULTIPLY, kMicroSecsPerSec}},
        {{hdk::ir::TimeUnit::kMicro, kMILLISECOND}, {kDIVIDE, kMilliSecsPerSec}},
        {{hdk::ir::TimeUnit::kMicro, kNANOSECOND}, {kMULTIPLY, kMilliSecsPerSec}},
        {{hdk::ir::TimeUnit::kNano, kMILLISECOND}, {kDIVIDE, kMicroSecsPerSec}},
        {{hdk::ir::TimeUnit::kNano, kMICROSECOND}, {kDIVIDE, kMilliSecsPerSec}}};

static const std::map<std::pair<hdk::ir::TimeUnit, DatetruncField>, int64_t>
    datetrunc_precision_lookup = {
        {{hdk::ir::TimeUnit::kMicro, dtMILLISECOND}, kMilliSecsPerSec},
        {{hdk::ir::TimeUnit::kNano, dtMICROSECOND}, kMilliSecsPerSec},
        {{hdk::ir::TimeUnit::kNano, dtMILLISECOND}, kMicroSecsPerSec}};

}  // namespace

namespace DateTimeUtils {

// Enum helpers for precision scaling up/down.
enum ScalingType { ScaleUp, ScaleDown };

constexpr inline int64_t get_timestamp_precision_scale(const int32_t dimen) {
  switch (dimen) {
    case 0:
      return 1;
    case 3:
      return kMilliSecsPerSec;
    case 6:
      return kMicroSecsPerSec;
    case 9:
      return kNanoSecsPerSec;
    default:
      throw std::runtime_error("Unknown dimen = " + std::to_string(dimen));
  }
  return -1;
}

constexpr inline int64_t get_dateadd_timestamp_precision_scale(const DateaddField field) {
  switch (field) {
    case daMILLISECOND:
      return kMilliSecsPerSec;
    case daMICROSECOND:
      return kMicroSecsPerSec;
    case daNANOSECOND:
      return kNanoSecsPerSec;
    default:
      throw std::runtime_error("Unknown field = " + std::to_string(field));
  }
  return -1;
}

constexpr inline int64_t get_extract_timestamp_precision_scale(const ExtractField field) {
  switch (field) {
    case kMILLISECOND:
      return kMilliSecsPerSec;
    case kMICROSECOND:
      return kMicroSecsPerSec;
    case kNANOSECOND:
      return kNanoSecsPerSec;
    default:
      throw std::runtime_error("Unknown field = " + std::to_string(field));
  }
  return -1;
}

constexpr inline bool is_subsecond_extract_field(const ExtractField& field) {
  return field == kMILLISECOND || field == kMICROSECOND || field == kNANOSECOND;
}

constexpr inline bool is_subsecond_dateadd_field(const DateaddField field) {
  return field == daMILLISECOND || field == daMICROSECOND || field == daNANOSECOND;
}

constexpr inline bool is_subsecond_datetrunc_field(const DatetruncField field) {
  return field == dtMILLISECOND || field == dtMICROSECOND || field == dtNANOSECOND;
}

const inline std::pair<SQLOps, int64_t> get_dateadd_high_precision_adjusted_scale(
    const DateaddField field,
    int32_t dimen) {
  switch (field) {
    case daNANOSECOND:
      switch (dimen) {
        case 9:
          return {};
        case 6:
          return {kDIVIDE, kMilliSecsPerSec};
        case 3:
          return {kDIVIDE, kMicroSecsPerSec};
        default:
          throw std::runtime_error("Unknown dimen = " + std::to_string(dimen));
      }
    case daMICROSECOND:
      switch (dimen) {
        case 9:
          return {kMULTIPLY, kMilliSecsPerSec};
        case 6:
          return {};
        case 3:
          return {kDIVIDE, kMilliSecsPerSec};
        default:
          throw std::runtime_error("Unknown dimen = " + std::to_string(dimen));
      }
    case daMILLISECOND:
      switch (dimen) {
        case 9:
          return {kMULTIPLY, kMicroSecsPerSec};
        case 6:
          return {kMULTIPLY, kMilliSecsPerSec};
        case 3:
          return {};
        default:
          throw std::runtime_error("Unknown dimen = " + std::to_string(dimen));
      }
    default:
      throw std::runtime_error("Unknown field = " + std::to_string(field));
  }
  return {};
}

const inline std::pair<SQLOps, int64_t> get_extract_high_precision_adjusted_scale(
    const ExtractField& field,
    const hdk::ir::TimeUnit unit) {
  const auto result = extract_precision_lookup.find(std::make_pair(unit, field));
  if (result != extract_precision_lookup.end()) {
    return result->second;
  }
  return {};
}

const inline int64_t get_datetrunc_high_precision_scale(const DatetruncField& field,
                                                        const hdk::ir::TimeUnit unit) {
  const auto result = datetrunc_precision_lookup.find(std::make_pair(unit, field));
  if (result != datetrunc_precision_lookup.end()) {
    return result->second;
  }
  return -1;
}

constexpr inline int64_t get_datetime_scaled_epoch(const ScalingType direction,
                                                   const int64_t epoch,
                                                   const int32_t dimen) {
  switch (direction) {
    case ScaleUp: {
      const auto scaled_epoch = epoch * get_timestamp_precision_scale(dimen);
      if (epoch && epoch != scaled_epoch / get_timestamp_precision_scale(dimen)) {
        throw std::runtime_error(
            "Value Overflow/underflow detected while scaling DateTime precision.");
      }
      return scaled_epoch;
    }
    case ScaleDown:
      return epoch / get_timestamp_precision_scale(dimen);
    default:
      abort();
  }
  return std::numeric_limits<int64_t>::min();
}

constexpr inline int64_t get_nanosecs_in_unit(hdk::ir::TimeUnit unit) {
  switch (unit) {
    case hdk::ir::TimeUnit::kDay:
      return 86'400'000'000'000;
    case hdk::ir::TimeUnit::kSecond:
      return 1'000'000'000;
    case hdk::ir::TimeUnit::kMilli:
      return 1'000'000;
    case hdk::ir::TimeUnit::kMicro:
      return 1'000;
    case hdk::ir::TimeUnit::kNano:
      return 1;
    default:
      throw std::runtime_error("Unexpected time unit: " + toString(unit));
  }
  return -1;
}

constexpr inline int64_t get_datetime_scaled_epoch(int64_t epoch,
                                                   hdk::ir::TimeUnit old_unit,
                                                   hdk::ir::TimeUnit new_unit) {
  auto old_scale = get_nanosecs_in_unit(old_unit);
  auto new_scale = get_nanosecs_in_unit(new_unit);
  if (old_scale > new_scale) {
    auto scaled_epoch = epoch * (old_scale / new_scale);
    if (epoch && epoch != scaled_epoch / (old_scale / new_scale)) {
      throw std::runtime_error(
          "Value Overflow/underflow detected while scaling DateTime precision.");
    }
    return scaled_epoch;
  } else {
    return epoch / (new_scale / old_scale);
  }
}

}  // namespace DateTimeUtils
