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

#include "CodeGenerator.h"
#include "Execute.h"
#include "ExprByPredicateVisitor.h"
#include "NullableValue.h"

#include <llvm/IR/MDBuilder.h>

namespace {

bool contains_unsafe_division(const hdk::ir::Expr* expr) {
  auto is_div = [](const hdk::ir::Expr* e) -> bool {
    auto bin_oper = dynamic_cast<const hdk::ir::BinOper*>(e);
    if (bin_oper && bin_oper->isDivide()) {
      auto rhs = bin_oper->get_right_operand();
      auto rhs_constant = dynamic_cast<const hdk::ir::Constant*>(rhs);
      if (!rhs_constant || rhs_constant->isNull()) {
        return true;
      }
      const auto& datum = rhs_constant->value();
      auto type = rhs_constant->type();
      if ((type->isBoolean() && datum.boolval == 0) ||
          (type->isInt8() && datum.tinyintval == 0) ||
          (type->isInt16() && datum.smallintval == 0) ||
          (type->isInt32() && datum.intval == 0) ||
          (type->isInt64() && datum.bigintval == 0LL) ||
          (type->isDecimal() && datum.bigintval == 0LL) ||
          (type->isFp32() && datum.floatval == 0.0) ||
          (type->isFp64() && datum.doubleval == 0.0)) {
        return true;
      }
    }
    return false;
  };
  std::list<const hdk::ir::Expr*> binoper_list =
      ExprByPredicateVisitor::collect(expr, is_div);
  return !binoper_list.empty();
}

bool should_defer_eval(const hdk::ir::ExprPtr expr) {
  if (expr->is<hdk::ir::LikeExpr>()) {
    return true;
  }
  if (expr->is<hdk::ir::RegexpExpr>()) {
    return true;
  }
  if (expr->is<hdk::ir::FunctionOper>()) {
    return true;
  }
  if (!expr->is<hdk::ir::BinOper>()) {
    return false;
  }
  const auto bin_expr = expr->as<hdk::ir::BinOper>();
  if (contains_unsafe_division(bin_expr)) {
    return true;
  }
  const auto rhs = bin_expr->get_right_operand();
  return rhs->type()->isArray();
}

Likelihood get_likelihood(const hdk::ir::Expr* expr) {
  Likelihood truth{1.0};
  auto likelihood_expr = dynamic_cast<const hdk::ir::LikelihoodExpr*>(expr);
  if (likelihood_expr) {
    return Likelihood(likelihood_expr->get_likelihood());
  }
  auto u_oper = dynamic_cast<const hdk::ir::UOper*>(expr);
  if (u_oper) {
    Likelihood oper_likelihood = get_likelihood(u_oper->operand());
    if (oper_likelihood.isInvalid()) {
      return Likelihood();
    }
    if (u_oper->isNot()) {
      return truth - oper_likelihood;
    }
    return oper_likelihood;
  }
  auto bin_oper = dynamic_cast<const hdk::ir::BinOper*>(expr);
  if (bin_oper) {
    auto lhs = bin_oper->get_left_operand();
    auto rhs = bin_oper->get_right_operand();
    Likelihood lhs_likelihood = get_likelihood(lhs);
    Likelihood rhs_likelihood = get_likelihood(rhs);
    if (lhs_likelihood.isInvalid() && rhs_likelihood.isInvalid()) {
      return Likelihood();
    }
    if (bin_oper->isOr()) {
      auto both_false = (truth - lhs_likelihood) * (truth - rhs_likelihood);
      return truth - both_false;
    }
    if (bin_oper->isAnd()) {
      return lhs_likelihood * rhs_likelihood;
    }
    return (lhs_likelihood + rhs_likelihood) / 2.0;
  }

  return Likelihood();
}

Weight get_weight(const hdk::ir::Expr* expr, int depth = 0) {
  auto like_expr = dynamic_cast<const hdk::ir::LikeExpr*>(expr);
  if (like_expr) {
    // heavy weight expr, start valid weight propagation
    return Weight((like_expr->get_is_simple()) ? 200 : 1000);
  }
  auto regexp_expr = dynamic_cast<const hdk::ir::RegexpExpr*>(expr);
  if (regexp_expr) {
    // heavy weight expr, start valid weight propagation
    return Weight(2000);
  }
  auto u_oper = dynamic_cast<const hdk::ir::UOper*>(expr);
  if (u_oper) {
    auto weight = get_weight(u_oper->operand(), depth + 1);
    return weight + 1;
  }
  auto bin_oper = dynamic_cast<const hdk::ir::BinOper*>(expr);
  if (bin_oper) {
    auto lhs = bin_oper->get_left_operand();
    auto rhs = bin_oper->get_right_operand();
    auto lhs_weight = get_weight(lhs, depth + 1);
    auto rhs_weight = get_weight(rhs, depth + 1);
    if (rhs->type()->isArray()) {
      // heavy weight expr, start valid weight propagation
      rhs_weight = rhs_weight + Weight(100);
    }
    auto weight = lhs_weight + rhs_weight;
    return weight + 1;
  }

  if (depth > 4) {
    return Weight(1);
  }

  return Weight();
}

}  // namespace

bool CodeGenerator::prioritizeQuals(const RelAlgExecutionUnit& ra_exe_unit,
                                    std::vector<const hdk::ir::Expr*>& primary_quals,
                                    std::vector<const hdk::ir::Expr*>& deferred_quals,
                                    const PlanState::HoistedFiltersSet& hoisted_quals) {
  for (auto expr : ra_exe_unit.simple_quals) {
    if (hoisted_quals.find(expr) != hoisted_quals.end()) {
      continue;
    }
    if (should_defer_eval(expr)) {
      deferred_quals.push_back(expr.get());
      continue;
    }
    primary_quals.push_back(expr.get());
  }

  bool short_circuit = false;

  for (auto expr : ra_exe_unit.quals) {
    if (hoisted_quals.find(expr) != hoisted_quals.end()) {
      continue;
    }

    if (get_likelihood(expr.get()) < 0.10 && !contains_unsafe_division(expr.get())) {
      if (!short_circuit) {
        primary_quals.push_back(expr.get());
        short_circuit = true;
        continue;
      }
    }
    if (short_circuit || should_defer_eval(expr)) {
      deferred_quals.push_back(expr.get());
      continue;
    }
    primary_quals.push_back(expr.get());
  }

  return short_circuit;
}

llvm::Value* CodeGenerator::codegenLogicalShortCircuit(const hdk::ir::BinOper* bin_oper,
                                                       const CompilationOptions& co) {
  AUTOMATIC_IR_METADATA(cgen_state_);
  const auto optype = bin_oper->opType();
  auto lhs = bin_oper->get_left_operand();
  auto rhs = bin_oper->get_right_operand();

  if (contains_unsafe_division(rhs)) {
    // rhs contains a possible div-by-0: short-circuit
  } else if (contains_unsafe_division(lhs)) {
    // lhs contains a possible div-by-0: swap and short-circuit
    std::swap(rhs, lhs);
  } else if (((optype == kOR && get_likelihood(lhs) > 0.90) ||
              (optype == kAND && get_likelihood(lhs) < 0.10)) &&
             get_weight(rhs) > 10) {
    // short circuit if we're likely to see either (trueA || heavyB) or (falseA && heavyB)
  } else if (((optype == kOR && get_likelihood(rhs) > 0.90) ||
              (optype == kAND && get_likelihood(rhs) < 0.10)) &&
             get_weight(lhs) > 10) {
    // swap and short circuit if we're likely to see either (heavyA || trueB) or (heavyA
    // && falseB)
    std::swap(rhs, lhs);
  } else {
    // no motivation to short circuit
    return nullptr;
  }

  auto type = bin_oper->type();
  auto lhs_lv = codegen(lhs, true, co).front();

  // Here the linear control flow will diverge and expressions cached during the
  // code branch code generation (currently just column decoding) are not going
  // to be available once we're done generating the short-circuited logic.
  // Take a snapshot of the cache with FetchCacheAnchor and restore it once
  // the control flow converges.
  Executor::FetchCacheAnchor anchor(cgen_state_);

  auto rhs_bb = llvm::BasicBlock::Create(
      cgen_state_->context_, "rhs_bb", cgen_state_->current_func_);
  auto ret_bb = llvm::BasicBlock::Create(
      cgen_state_->context_, "ret_bb", cgen_state_->current_func_);
  llvm::BasicBlock* nullcheck_ok_bb{nullptr};
  llvm::BasicBlock* nullcheck_fail_bb{nullptr};

  if (type->nullable()) {
    // need lhs nullcheck before short circuiting
    nullcheck_ok_bb = llvm::BasicBlock::Create(
        cgen_state_->context_, "nullcheck_ok_bb", cgen_state_->current_func_);
    nullcheck_fail_bb = llvm::BasicBlock::Create(
        cgen_state_->context_, "nullcheck_fail_bb", cgen_state_->current_func_);
    if (lhs_lv->getType()->isIntegerTy(1)) {
      lhs_lv = cgen_state_->castToTypeIn(lhs_lv, 8);
    }
    auto lhs_nullcheck =
        cgen_state_->ir_builder_.CreateICmpEQ(lhs_lv, cgen_state_->inlineIntNull(type));
    cgen_state_->ir_builder_.CreateCondBr(
        lhs_nullcheck, nullcheck_fail_bb, nullcheck_ok_bb);
    cgen_state_->ir_builder_.SetInsertPoint(nullcheck_ok_bb);
  }

  auto sc_check_bb = cgen_state_->ir_builder_.GetInsertBlock();
  auto cnst_lv = llvm::ConstantInt::get(lhs_lv->getType(), (optype == kOR));
  // Branch to codegen rhs if NOT getting (true || rhs) or (false && rhs), likelihood of
  // the branch is < 0.10
  cgen_state_->ir_builder_.CreateCondBr(
      cgen_state_->ir_builder_.CreateICmpNE(lhs_lv, cnst_lv),
      rhs_bb,
      ret_bb,
      llvm::MDBuilder(cgen_state_->context_).createBranchWeights(10, 90));

  // Codegen rhs when unable to short circuit.
  cgen_state_->ir_builder_.SetInsertPoint(rhs_bb);
  auto rhs_lv = codegen(rhs, true, co).front();
  if (type->nullable()) {
    // need rhs nullcheck as well
    if (rhs_lv->getType()->isIntegerTy(1)) {
      rhs_lv = cgen_state_->castToTypeIn(rhs_lv, 8);
    }
    auto rhs_nullcheck =
        cgen_state_->ir_builder_.CreateICmpEQ(rhs_lv, cgen_state_->inlineIntNull(type));
    cgen_state_->ir_builder_.CreateCondBr(rhs_nullcheck, nullcheck_fail_bb, ret_bb);
  } else {
    cgen_state_->ir_builder_.CreateBr(ret_bb);
  }
  auto rhs_codegen_bb = cgen_state_->ir_builder_.GetInsertBlock();

  if (type->nullable()) {
    cgen_state_->ir_builder_.SetInsertPoint(nullcheck_fail_bb);
    cgen_state_->ir_builder_.CreateBr(ret_bb);
  }

  cgen_state_->ir_builder_.SetInsertPoint(ret_bb);
  auto result_phi =
      cgen_state_->ir_builder_.CreatePHI(lhs_lv->getType(), type->nullable() ? 3 : 2);
  if (type->nullable()) {
    result_phi->addIncoming(cgen_state_->inlineIntNull(type), nullcheck_fail_bb);
  }
  result_phi->addIncoming(cnst_lv, sc_check_bb);
  result_phi->addIncoming(rhs_lv, rhs_codegen_bb);
  return result_phi;
}

llvm::Value* CodeGenerator::codegenLogical(const hdk::ir::BinOper* bin_oper,
                                           const CompilationOptions& co) {
  AUTOMATIC_IR_METADATA(cgen_state_);
  const auto optype = bin_oper->opType();
  CHECK(bin_oper->isLogic());

  if (llvm::Value* short_circuit = codegenLogicalShortCircuit(bin_oper, co)) {
    return short_circuit;
  }

  const auto lhs = bin_oper->get_left_operand();
  const auto rhs = bin_oper->get_right_operand();
  auto lhs_lv = codegen(lhs, true, co).front();
  auto rhs_lv = codegen(rhs, true, co).front();
  auto type = bin_oper->type();
  if (!type->nullable()) {
    switch (optype) {
      case kAND:
        return cgen_state_->ir_builder_.CreateAnd(toBool(lhs_lv), toBool(rhs_lv));
      case kOR:
        return cgen_state_->ir_builder_.CreateOr(toBool(lhs_lv), toBool(rhs_lv));
      default:
        CHECK(false);
    }
  }
  CHECK(lhs_lv->getType()->isIntegerTy(1) || lhs_lv->getType()->isIntegerTy(8));
  CHECK(rhs_lv->getType()->isIntegerTy(1) || rhs_lv->getType()->isIntegerTy(8));
  if (lhs_lv->getType()->isIntegerTy(1)) {
    lhs_lv = cgen_state_->castToTypeIn(lhs_lv, 8);
  }
  if (rhs_lv->getType()->isIntegerTy(1)) {
    rhs_lv = cgen_state_->castToTypeIn(rhs_lv, 8);
  }
  switch (optype) {
    case kAND:
      return cgen_state_->emitCall("logical_and",
                                   {lhs_lv, rhs_lv, cgen_state_->inlineIntNull(type)});
    case kOR:
      return cgen_state_->emitCall("logical_or",
                                   {lhs_lv, rhs_lv, cgen_state_->inlineIntNull(type)});
    default:
      abort();
  }
}

llvm::Value* CodeGenerator::toBool(llvm::Value* lv) {
  AUTOMATIC_IR_METADATA(cgen_state_);
  CHECK(lv->getType()->isIntegerTy());
  if (static_cast<llvm::IntegerType*>(lv->getType())->getBitWidth() > 1) {
    return cgen_state_->ir_builder_.CreateICmp(
        llvm::ICmpInst::ICMP_SGT, lv, llvm::ConstantInt::get(lv->getType(), 0));
  }
  return lv;
}

namespace {

bool is_qualified_bin_oper(const hdk::ir::Expr* expr) {
  const auto bin_oper = dynamic_cast<const hdk::ir::BinOper*>(expr);
  return bin_oper && bin_oper->qualifier() != kONE;
}

}  // namespace

llvm::Value* CodeGenerator::codegenLogical(const hdk::ir::UOper* uoper,
                                           const CompilationOptions& co) {
  AUTOMATIC_IR_METADATA(cgen_state_);
  CHECK(uoper->isNot());
  const auto operand = uoper->operand();
  auto operand_type = operand->type();
  CHECK(operand_type->isBoolean());
  const auto operand_lv = codegen(operand, true, co).front();
  CHECK(operand_lv->getType()->isIntegerTy());
  const bool not_null = (!operand_type->nullable() || is_qualified_bin_oper(operand));
  CHECK(not_null || operand_lv->getType()->isIntegerTy(8));
  return not_null
             ? cgen_state_->ir_builder_.CreateNot(toBool(operand_lv))
             : cgen_state_->emitCall(
                   "logical_not", {operand_lv, cgen_state_->inlineIntNull(operand_type)});
}

llvm::Value* CodeGenerator::codegenIsNull(const hdk::ir::UOper* uoper,
                                          const CompilationOptions& co) {
  AUTOMATIC_IR_METADATA(cgen_state_);
  const auto operand = uoper->operand();
  if (dynamic_cast<const hdk::ir::Constant*>(operand) &&
      dynamic_cast<const hdk::ir::Constant*>(operand)->isNull()) {
    // for null constants, short-circuit to true
    return llvm::ConstantInt::get(get_int_type(1, cgen_state_->context_), 1);
  }
  auto type = operand->type();
  CHECK(type->isNumber() || type->isBoolean() || type->isDateTime() || type->isString() ||
        type->isExtDictionary() || type->isArray());
  // if the type is inferred as non null, short-circuit to false
  if (!type->nullable()) {
    return llvm::ConstantInt::get(get_int_type(1, cgen_state_->context_), 0);
  }
  const auto operand_lv = codegen(operand, true, co).front();
  // NULL-check array
  if (type->isArray()) {
    auto fname = "array_is_null";
    return cgen_state_->emitExternalCall(
        fname, get_int_type(1, cgen_state_->context_), {operand_lv, posArg(operand)});
  }
  return codegenIsNullNumber(operand_lv, type);
}

llvm::Value* CodeGenerator::codegenIsNullNumber(llvm::Value* operand_lv,
                                                const hdk::ir::Type* type) {
  AUTOMATIC_IR_METADATA(cgen_state_);
  if (type->isFloatingPoint()) {
    return cgen_state_->ir_builder_.CreateFCmp(llvm::FCmpInst::FCMP_OEQ,
                                               operand_lv,
                                               type->size() == 4
                                                   ? cgen_state_->llFp(NULL_FLOAT)
                                                   : cgen_state_->llFp(NULL_DOUBLE));
  }
  return cgen_state_->ir_builder_.CreateICmp(
      llvm::ICmpInst::ICMP_EQ, operand_lv, cgen_state_->inlineIntNull(type));
}
