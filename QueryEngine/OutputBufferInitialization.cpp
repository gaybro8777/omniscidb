/*
 * Copyright 2017 MapD Technologies, Inc.
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

#include "OutputBufferInitialization.h"
#include "BufferCompaction.h"
#include "Descriptors/QueryMemoryDescriptor.h"
#include "TypePunning.h"

#include "../Analyzer/Analyzer.h"

std::vector<int64_t> init_agg_val_vec(const std::vector<TargetInfo>& targets,
                                      const QueryMemoryDescriptor& query_mem_desc) {
  std::vector<int64_t> agg_init_vals;
  agg_init_vals.reserve(query_mem_desc.getSlotCount());
  const bool is_group_by{query_mem_desc.isGroupBy()};
  for (size_t target_idx = 0, agg_col_idx = 0; target_idx < targets.size();
       ++target_idx, ++agg_col_idx) {
    CHECK_LT(agg_col_idx, query_mem_desc.getSlotCount());
    const auto agg_info = targets[target_idx];
    auto agg_type = agg_info.type;
    if (!agg_info.is_agg || agg_info.agg_kind == kSAMPLE) {
      if (agg_info.agg_kind == kSAMPLE && agg_type->isExtDictionary()) {
        agg_init_vals.push_back(
            get_agg_initial_val(agg_info.agg_kind,
                                agg_type,
                                is_group_by,
                                query_mem_desc.getCompactByteWidth()));
        continue;
      }
      if (query_mem_desc.getPaddedSlotWidthBytes(agg_col_idx) > 0) {
        agg_init_vals.push_back(0);
      }
      if (agg_type->isArray() || agg_type->isString()) {
        agg_init_vals.push_back(0);
      }
      continue;
    }
    CHECK_GT(query_mem_desc.getPaddedSlotWidthBytes(agg_col_idx), 0);
    const bool float_argument_input = takes_float_argument(agg_info);
    const auto chosen_bytes = query_mem_desc.isLogicalSizedColumnsAllowed()
                                  ? query_mem_desc.getPaddedSlotWidthBytes(agg_col_idx)
                                  : query_mem_desc.getCompactByteWidth();
    auto init_type = get_compact_type(agg_info);
    if (!is_group_by) {
      init_type = init_type->withNullable(true);
    }
    agg_init_vals.push_back(
        get_agg_initial_val(agg_info.agg_kind,
                            init_type,
                            is_group_by || float_argument_input,
                            (float_argument_input ? sizeof(float) : chosen_bytes)));
    if (kAVG == agg_info.agg_kind) {
      ++agg_col_idx;
      agg_init_vals.push_back(0);
    }
  }
  return agg_init_vals;
}

std::pair<int64_t, int64_t> inline_int_max_min(const size_t byte_width) {
  switch (byte_width) {
    case 1:
      return std::make_pair(std::numeric_limits<int8_t>::max(),
                            std::numeric_limits<int8_t>::min());
    case 2:
      return std::make_pair(std::numeric_limits<int16_t>::max(),
                            std::numeric_limits<int16_t>::min());
    case 4:
      return std::make_pair(std::numeric_limits<int32_t>::max(),
                            std::numeric_limits<int32_t>::min());
    case 8:
      return std::make_pair(std::numeric_limits<int64_t>::max(),
                            std::numeric_limits<int64_t>::min());
    default:
      abort();
  }
}

std::pair<uint64_t, uint64_t> inline_uint_max_min(const size_t byte_width) {
  switch (byte_width) {
    case 1:
      return std::make_pair(std::numeric_limits<uint8_t>::max(),
                            std::numeric_limits<uint8_t>::min());
    case 2:
      return std::make_pair(std::numeric_limits<uint16_t>::max(),
                            std::numeric_limits<uint16_t>::min());
    case 4:
      return std::make_pair(std::numeric_limits<uint32_t>::max(),
                            std::numeric_limits<uint32_t>::min());
    case 8:
      return std::make_pair(std::numeric_limits<uint64_t>::max(),
                            std::numeric_limits<uint64_t>::min());
    default:
      abort();
  }
}

// TODO(alex): proper types for aggregate
int64_t get_agg_initial_val(const SQLAgg agg,
                            const hdk::ir::Type* type,
                            const bool enable_compaction,
                            const unsigned min_byte_width_to_compact) {
  CHECK(!(type->isString() || type->isExtDictionary()) ||
        (agg == kSINGLE_VALUE || agg == kSAMPLE));
  const auto byte_width =
      enable_compaction
          ? compact_byte_width(static_cast<unsigned>(get_bit_width(type) >> 3),
                               unsigned(min_byte_width_to_compact))
          : sizeof(int64_t);
  CHECK(type->canonicalSize() < 0 ||
        byte_width >= static_cast<unsigned>(type->canonicalSize()));
  switch (agg) {
    case kSUM: {
      if (type->nullable()) {
        if (type->isFloatingPoint()) {
          switch (byte_width) {
            case 4: {
              const float null_float = inline_fp_null_value(type);
              return *reinterpret_cast<const int32_t*>(may_alias_ptr(&null_float));
            }
            case 8: {
              const double null_double = inline_fp_null_value(type);
              return *reinterpret_cast<const int64_t*>(may_alias_ptr(&null_double));
            }
            default:
              CHECK(false);
          }
        } else {
          return inline_int_null_value(type);
        }
      }
      switch (byte_width) {
        case 4: {
          const float zero_float{0.};
          return type->isFloatingPoint()
                     ? *reinterpret_cast<const int32_t*>(may_alias_ptr(&zero_float))
                     : 0;
        }
        case 8: {
          const double zero_double{0.};
          return type->isFloatingPoint()
                     ? *reinterpret_cast<const int64_t*>(may_alias_ptr(&zero_double))
                     : 0;
        }
        default:
          CHECK(false);
      }
    }
    case kAVG:
    case kCOUNT:
    case kAPPROX_COUNT_DISTINCT:
      return 0;
    case kAPPROX_QUANTILE:
      return {};  // Init value is a quantile::TDigest* set elsewhere.
    case kMIN: {
      switch (byte_width) {
        case 1: {
          CHECK(!type->isFloatingPoint());
          return !type->nullable() ? std::numeric_limits<int8_t>::max()
                                   : inline_int_null_value(type);
        }
        case 2: {
          CHECK(!type->isFloatingPoint());
          return !type->nullable() ? std::numeric_limits<int16_t>::max()
                                   : inline_int_null_value(type);
        }
        case 4: {
          const float max_float = std::numeric_limits<float>::max();
          const float null_float = type->isFloatingPoint()
                                       ? static_cast<float>(inline_fp_null_value(type))
                                       : 0.;
          return type->isFloatingPoint()
                     ? (!type->nullable()
                            ? *reinterpret_cast<const int32_t*>(may_alias_ptr(&max_float))
                            : *reinterpret_cast<const int32_t*>(
                                  may_alias_ptr(&null_float)))
                     : (!type->nullable() ? std::numeric_limits<int32_t>::max()
                                          : inline_int_null_value(type));
        }
        case 8: {
          const double max_double = std::numeric_limits<double>::max();
          const double null_double{type->isFloatingPoint() ? inline_fp_null_value(type)
                                                           : 0.};
          return type->isFloatingPoint()
                     ? (!type->nullable() ? *reinterpret_cast<const int64_t*>(
                                                may_alias_ptr(&max_double))
                                          : *reinterpret_cast<const int64_t*>(
                                                may_alias_ptr(&null_double)))
                     : (!type->nullable() ? std::numeric_limits<int64_t>::max()
                                          : inline_int_null_value(type));
        }
        default:
          CHECK(false);
      }
    }
    case kSINGLE_VALUE:
    case kSAMPLE:
    case kMAX: {
      switch (byte_width) {
        case 1: {
          CHECK(!type->isFloatingPoint());
          return !type->nullable() ? std::numeric_limits<int8_t>::min()
                                   : inline_int_null_value(type);
        }
        case 2: {
          CHECK(!type->isFloatingPoint());
          return !type->nullable() ? std::numeric_limits<int16_t>::min()
                                   : inline_int_null_value(type);
        }
        case 4: {
          const float min_float = -std::numeric_limits<float>::max();
          const float null_float = type->isFloatingPoint()
                                       ? static_cast<float>(inline_fp_null_value(type))
                                       : 0.;
          return (type->isFloatingPoint())
                     ? (!type->nullable()
                            ? *reinterpret_cast<const int32_t*>(may_alias_ptr(&min_float))
                            : *reinterpret_cast<const int32_t*>(
                                  may_alias_ptr(&null_float)))
                     : (!type->nullable() ? std::numeric_limits<int32_t>::min()
                                          : inline_int_null_value(type));
        }
        case 8: {
          const double min_double = -std::numeric_limits<double>::max();
          const double null_double{type->isFloatingPoint() ? inline_fp_null_value(type)
                                                           : 0.};
          return type->isFloatingPoint()
                     ? (!type->nullable() ? *reinterpret_cast<const int64_t*>(
                                                may_alias_ptr(&min_double))
                                          : *reinterpret_cast<const int64_t*>(
                                                may_alias_ptr(&null_double)))
                     : (!type->nullable() ? std::numeric_limits<int64_t>::min()
                                          : inline_int_null_value(type));
        }
        default:
          CHECK(false);
      }
    }
    default:
      abort();
  }
}

std::vector<int64_t> init_agg_val_vec(const std::vector<hdk::ir::Expr*>& targets,
                                      const std::list<hdk::ir::ExprPtr>& quals,
                                      const QueryMemoryDescriptor& query_mem_desc,
                                      bool bigint_count) {
  std::vector<TargetInfo> target_infos;
  target_infos.reserve(targets.size());
  const auto agg_col_count = query_mem_desc.getSlotCount();
  for (size_t target_idx = 0, agg_col_idx = 0;
       target_idx < targets.size() && agg_col_idx < agg_col_count;
       ++target_idx, ++agg_col_idx) {
    const auto target_expr = targets[target_idx];
    auto target = get_target_info(target_expr, bigint_count);
    auto arg_expr = agg_arg(target_expr);
    if (arg_expr) {
      if (query_mem_desc.getQueryDescriptionType() ==
              QueryDescriptionType::NonGroupedAggregate &&
          target.is_agg &&
          (target.agg_kind == kMIN || target.agg_kind == kMAX ||
           target.agg_kind == kSUM || target.agg_kind == kAVG ||
           target.agg_kind == kAPPROX_QUANTILE)) {
        set_notnull(target, false);
      } else if (constrained_not_null(arg_expr, quals)) {
        set_notnull(target, true);
      }
    }
    target_infos.push_back(target);
  }
  return init_agg_val_vec(target_infos, query_mem_desc);
}

const hdk::ir::Expr* agg_arg(const hdk::ir::Expr* expr) {
  const auto agg_expr = dynamic_cast<const hdk::ir::AggExpr*>(expr);
  return agg_expr ? agg_expr->get_arg() : nullptr;
}

bool constrained_not_null(const hdk::ir::Expr* expr,
                          const std::list<hdk::ir::ExprPtr>& quals) {
  for (const auto& qual : quals) {
    auto uoper = std::dynamic_pointer_cast<hdk::ir::UOper>(qual);
    if (!uoper) {
      continue;
    }
    bool is_negated{false};
    if (uoper->get_optype() == kNOT) {
      uoper = std::dynamic_pointer_cast<hdk::ir::UOper>(uoper->get_own_operand());
      is_negated = true;
    }
    if (uoper && (uoper->get_optype() == kISNOTNULL ||
                  (is_negated && uoper->get_optype() == kISNULL))) {
      if (*uoper->get_own_operand() == *expr) {
        return true;
      }
    }
  }
  return false;
}
