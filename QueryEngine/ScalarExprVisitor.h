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

#ifndef QUERYENGINE_SCALAREXPRVISITOR_H
#define QUERYENGINE_SCALAREXPRVISITOR_H

#include "IR/Expr.h"

template <class T>
class ScalarExprVisitor {
 public:
  virtual ~ScalarExprVisitor() {}

  virtual T visit(const hdk::ir::Expr* expr) const {
    CHECK(expr);
    const auto var = dynamic_cast<const hdk::ir::Var*>(expr);
    if (var) {
      return visitVar(var);
    }
    const auto column_var = dynamic_cast<const hdk::ir::ColumnVar*>(expr);
    if (column_var) {
      return visitColumnVar(column_var);
    }
    const auto column_ref = dynamic_cast<const hdk::ir::ColumnRef*>(expr);
    if (column_ref) {
      return visitColumnRef(column_ref);
    }
    const auto group_column_ref = dynamic_cast<const hdk::ir::GroupColumnRef*>(expr);
    if (group_column_ref) {
      return visitGroupColumnRef(group_column_ref);
    }
    const auto column_var_tuple = dynamic_cast<const hdk::ir::ExpressionTuple*>(expr);
    if (column_var_tuple) {
      return visitColumnVarTuple(column_var_tuple);
    }
    const auto constant = dynamic_cast<const hdk::ir::Constant*>(expr);
    if (constant) {
      return visitConstant(constant);
    }
    const auto uoper = dynamic_cast<const hdk::ir::UOper*>(expr);
    if (uoper) {
      return visitUOper(uoper);
    }
    const auto bin_oper = dynamic_cast<const hdk::ir::BinOper*>(expr);
    if (bin_oper) {
      return visitBinOper(bin_oper);
    }
    const auto scalar_subquery = dynamic_cast<const hdk::ir::ScalarSubquery*>(expr);
    if (scalar_subquery) {
      return visitScalarSubquery(scalar_subquery);
    }
    const auto in_values = dynamic_cast<const hdk::ir::InValues*>(expr);
    if (in_values) {
      return visitInValues(in_values);
    }
    const auto in_integer_set = dynamic_cast<const hdk::ir::InIntegerSet*>(expr);
    if (in_integer_set) {
      return visitInIntegerSet(in_integer_set);
    }
    const auto in_subquery = dynamic_cast<const hdk::ir::InSubquery*>(expr);
    if (in_subquery) {
      return visitInSubquery(in_subquery);
    }
    const auto char_length = dynamic_cast<const hdk::ir::CharLengthExpr*>(expr);
    if (char_length) {
      return visitCharLength(char_length);
    }
    const auto key_for_string = dynamic_cast<const hdk::ir::KeyForStringExpr*>(expr);
    if (key_for_string) {
      return visitKeyForString(key_for_string);
    }
    const auto sample_ratio = dynamic_cast<const hdk::ir::SampleRatioExpr*>(expr);
    if (sample_ratio) {
      return visitSampleRatio(sample_ratio);
    }
    const auto width_bucket = dynamic_cast<const hdk::ir::WidthBucketExpr*>(expr);
    if (width_bucket) {
      return visitWidthBucket(width_bucket);
    }
    const auto lower = dynamic_cast<const hdk::ir::LowerExpr*>(expr);
    if (lower) {
      return visitLower(lower);
    }
    const auto cardinality = dynamic_cast<const hdk::ir::CardinalityExpr*>(expr);
    if (cardinality) {
      return visitCardinality(cardinality);
    }
    const auto width_bucket_expr = dynamic_cast<const hdk::ir::WidthBucketExpr*>(expr);
    if (width_bucket_expr) {
      return visitWidthBucket(width_bucket_expr);
    }
    const auto like_expr = dynamic_cast<const hdk::ir::LikeExpr*>(expr);
    if (like_expr) {
      return visitLikeExpr(like_expr);
    }
    const auto regexp_expr = dynamic_cast<const hdk::ir::RegexpExpr*>(expr);
    if (regexp_expr) {
      return visitRegexpExpr(regexp_expr);
    }
    const auto case_ = dynamic_cast<const hdk::ir::CaseExpr*>(expr);
    if (case_) {
      return visitCaseExpr(case_);
    }
    const auto datetrunc = dynamic_cast<const hdk::ir::DatetruncExpr*>(expr);
    if (datetrunc) {
      return visitDatetruncExpr(datetrunc);
    }
    const auto extract = dynamic_cast<const hdk::ir::ExtractExpr*>(expr);
    if (extract) {
      return visitExtractExpr(extract);
    }
    const auto window_func = dynamic_cast<const hdk::ir::WindowFunction*>(expr);
    if (window_func) {
      return visitWindowFunction(window_func);
    }
    const auto func_with_custom_type_handling =
        dynamic_cast<const hdk::ir::FunctionOperWithCustomTypeHandling*>(expr);
    if (func_with_custom_type_handling) {
      return visitFunctionOperWithCustomTypeHandling(func_with_custom_type_handling);
    }
    const auto func = dynamic_cast<const hdk::ir::FunctionOper*>(expr);
    if (func) {
      return visitFunctionOper(func);
    }
    const auto array = dynamic_cast<const hdk::ir::ArrayExpr*>(expr);
    if (array) {
      return visitArrayOper(array);
    }
    const auto datediff = dynamic_cast<const hdk::ir::DatediffExpr*>(expr);
    if (datediff) {
      return visitDatediffExpr(datediff);
    }
    const auto dateadd = dynamic_cast<const hdk::ir::DateaddExpr*>(expr);
    if (dateadd) {
      return visitDateaddExpr(dateadd);
    }
    const auto likelihood = dynamic_cast<const hdk::ir::LikelihoodExpr*>(expr);
    if (likelihood) {
      return visitLikelihood(likelihood);
    }
    const auto offset_in_fragment = dynamic_cast<const hdk::ir::OffsetInFragment*>(expr);
    if (offset_in_fragment) {
      return visitOffsetInFragment(offset_in_fragment);
    }
    const auto agg = dynamic_cast<const hdk::ir::AggExpr*>(expr);
    if (agg) {
      return visitAggExpr(agg);
    }
    return defaultResult();
  }

 protected:
  virtual T visitVar(const hdk::ir::Var*) const { return defaultResult(); }

  virtual T visitColumnVar(const hdk::ir::ColumnVar*) const { return defaultResult(); }

  virtual T visitColumnRef(const hdk::ir::ColumnRef*) const { return defaultResult(); }

  virtual T visitGroupColumnRef(const hdk::ir::GroupColumnRef*) const {
    return defaultResult();
  }

  virtual T visitColumnVarTuple(const hdk::ir::ExpressionTuple*) const {
    return defaultResult();
  }

  virtual T visitConstant(const hdk::ir::Constant*) const { return defaultResult(); }

  virtual T visitUOper(const hdk::ir::UOper* uoper) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(uoper->operand()));
    return result;
  }

  virtual T visitBinOper(const hdk::ir::BinOper* bin_oper) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(bin_oper->leftOperand()));
    result = aggregateResult(result, visit(bin_oper->rightOperand()));
    return result;
  }

  virtual T visitScalarSubquery(const hdk::ir::ScalarSubquery* subquery) const {
    return defaultResult();
  }

  virtual T visitInValues(const hdk::ir::InValues* in_values) const {
    T result = visit(in_values->arg());
    const auto& value_list = in_values->valueList();
    for (const auto& in_value : value_list) {
      result = aggregateResult(result, visit(in_value.get()));
    }
    return result;
  }

  virtual T visitInIntegerSet(const hdk::ir::InIntegerSet* in_integer_set) const {
    return visit(in_integer_set->arg());
  }

  virtual T visitInSubquery(const hdk::ir::InSubquery* in_subquery) const {
    return visit(in_subquery->arg());
  }

  virtual T visitCharLength(const hdk::ir::CharLengthExpr* char_length) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(char_length->arg()));
    return result;
  }

  virtual T visitKeyForString(const hdk::ir::KeyForStringExpr* key_for_string) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(key_for_string->arg()));
    return result;
  }

  virtual T visitSampleRatio(const hdk::ir::SampleRatioExpr* sample_ratio) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(sample_ratio->arg()));
    return result;
  }

  virtual T visitLower(const hdk::ir::LowerExpr* lower_expr) const {
    return visit(lower_expr->arg());
  }

  virtual T visitCardinality(const hdk::ir::CardinalityExpr* cardinality) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(cardinality->arg()));
    return result;
  }

  virtual T visitLikeExpr(const hdk::ir::LikeExpr* like) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(like->arg()));
    result = aggregateResult(result, visit(like->likeExpr()));
    if (like->escapeExpr()) {
      result = aggregateResult(result, visit(like->escapeExpr()));
    }
    return result;
  }

  virtual T visitRegexpExpr(const hdk::ir::RegexpExpr* regexp) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(regexp->arg()));
    result = aggregateResult(result, visit(regexp->patternExpr()));
    if (regexp->get_escape_expr()) {
      result = aggregateResult(result, visit(regexp->get_escape_expr()));
    }
    return result;
  }

  virtual T visitWidthBucket(const hdk::ir::WidthBucketExpr* width_bucket_expr) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(width_bucket_expr->get_target_value()));
    result = aggregateResult(result, visit(width_bucket_expr->get_lower_bound()));
    result = aggregateResult(result, visit(width_bucket_expr->get_upper_bound()));
    result = aggregateResult(result, visit(width_bucket_expr->get_partition_count()));
    return result;
  }

  virtual T visitCaseExpr(const hdk::ir::CaseExpr* case_) const {
    T result = defaultResult();
    const auto& expr_pair_list = case_->get_expr_pair_list();
    for (const auto& expr_pair : expr_pair_list) {
      result = aggregateResult(result, visit(expr_pair.first.get()));
      result = aggregateResult(result, visit(expr_pair.second.get()));
    }
    result = aggregateResult(result, visit(case_->get_else_expr()));
    return result;
  }

  virtual T visitDatetruncExpr(const hdk::ir::DatetruncExpr* datetrunc) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(datetrunc->get_from_expr()));
    return result;
  }

  virtual T visitExtractExpr(const hdk::ir::ExtractExpr* extract) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(extract->get_from_expr()));
    return result;
  }

  virtual T visitFunctionOperWithCustomTypeHandling(
      const hdk::ir::FunctionOperWithCustomTypeHandling* func_oper) const {
    return visitFunctionOper(func_oper);
  }

  virtual T visitArrayOper(hdk::ir::ArrayExpr const* array_expr) const {
    T result = defaultResult();
    for (size_t i = 0; i < array_expr->getElementCount(); ++i) {
      result = aggregateResult(result, visit(array_expr->getElement(i)));
    }
    return result;
  }

  virtual T visitFunctionOper(const hdk::ir::FunctionOper* func_oper) const {
    T result = defaultResult();
    for (size_t i = 0; i < func_oper->getArity(); ++i) {
      result = aggregateResult(result, visit(func_oper->getArg(i)));
    }
    return result;
  }

  virtual T visitWindowFunction(const hdk::ir::WindowFunction* window_func) const {
    T result = defaultResult();
    for (const auto& arg : window_func->getArgs()) {
      result = aggregateResult(result, visit(arg.get()));
    }
    for (const auto& partition_key : window_func->getPartitionKeys()) {
      result = aggregateResult(result, visit(partition_key.get()));
    }
    for (const auto& order_key : window_func->getOrderKeys()) {
      result = aggregateResult(result, visit(order_key.get()));
    }
    return result;
  }

  virtual T visitDatediffExpr(const hdk::ir::DatediffExpr* datediff) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(datediff->get_start_expr()));
    result = aggregateResult(result, visit(datediff->get_end_expr()));
    return result;
  }

  virtual T visitDateaddExpr(const hdk::ir::DateaddExpr* dateadd) const {
    T result = defaultResult();
    result = aggregateResult(result, visit(dateadd->get_number_expr()));
    result = aggregateResult(result, visit(dateadd->get_datetime_expr()));
    return result;
  }

  virtual T visitLikelihood(const hdk::ir::LikelihoodExpr* likelihood) const {
    return visit(likelihood->get_arg());
  }

  virtual T visitOffsetInFragment(const hdk::ir::OffsetInFragment*) const {
    return defaultResult();
  }

  virtual T visitAggExpr(const hdk::ir::AggExpr* agg) const {
    if (agg->get_arg()) {
      return visit(agg->get_arg());
    }
    return defaultResult();
  }

 protected:
  virtual T aggregateResult(const T& aggregate, const T& next_result) const {
    return next_result;
  }

  virtual T defaultResult() const { return T{}; }
};

#endif  // QUERYENGINE_SCALAREXPRVISITOR_H
