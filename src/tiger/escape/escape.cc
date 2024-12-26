#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void check_escape_(esc::EscEnvPtr env, int depth, sym::Symbol *sym_ptr) {
  auto look_res = env->Look(sym_ptr);
  if (look_res && look_res->depth_ < depth && !*look_res->escape_) {
    *look_res->escape_ = true;
  }
}

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  this->root_->Traverse(env, 0);
  /* TODO: Put your lab5-part1 code here */
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  check_escape_(env, depth, this->sym_);
  /* TODO: Put your lab5-part1 code here */
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
  this->subscript_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->var_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5-part1 code here */
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5-part1 code here */
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5-part1 code here */
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (auto &&arg : this->args_->GetList()) {
    arg->Traverse(env, depth);
  }
  // static check for escape, don't need to exec function invocation
  /* TODO: Put your lab5-part1 code here */
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->left_->Traverse(env, depth);
  this->right_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (auto &&field : this->fields_->GetList()) {
    field->exp_->Traverse(env, depth);
  }
  /* TODO: Put your lab5-part1 code here */
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (auto &&exp : this->seq_->GetList()) {
    exp->Traverse(env, depth);
  }
  /* TODO: Put your lab5-part1 code here */
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->exp_->Traverse(env, depth);
  this->var_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->test_->Traverse(env, depth);
  this->then_->Traverse(env, depth);
  if (this->elsee_)
    this->elsee_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->test_->Traverse(env, depth);
  this->body_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  // what does the escape used for ?
  this->escape_ = false;
  this->lo_->Traverse(env, depth);
  this->hi_->Traverse(env, depth);
  env->BeginScope();
  env->Enter(this->var_, new esc::EscapeEntry(depth, &this->escape_));
  this->body_->Traverse(env, depth);
  env->EndScope();
  /* TODO: Put your lab5-part1 code here */
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5-part1 code here */
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  env->BeginScope();
  for (auto &&dec : this->decs_->GetList()) {
    dec->Traverse(env, depth);
  }
  this->body_->Traverse(env, depth);
  env->EndScope();
  /* TODO: Put your lab5-part1 code here */
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  this->init_->Traverse(env, depth); // to be checked
  this->size_->Traverse(env, depth);
  /* TODO: Put your lab5-part1 code here */
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5-part1 code here */
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {

  for (auto &&func : this->functions_->GetList()) {
    env->BeginScope();
    for (auto &&field : func->params_->GetList()) {
      field->escape_ = false;
      env->Enter(field->name_,
                 new esc::EscapeEntry(depth + 1, &field->escape_));
    }
    func->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
  /* TODO: Put your lab5-part1 code here */
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  this->escape_ = false;
  env->Enter(this->var_, new esc::EscapeEntry(depth, &this->escape_));
  /* TODO: Put your lab5-part1 code here */
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5-part1 code here */
}

} // namespace absyn
