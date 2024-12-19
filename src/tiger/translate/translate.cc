#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <stack>

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;
std::stack<llvm::Function *> func_stack;
std::stack<llvm::BasicBlock *> loop_stack;
llvm::Function *alloc_record;
llvm::Function *init_array;
llvm::Function *string_equal;
std::vector<std::pair<std::string, frame::Frame *>> frame_info;

bool CheckBBTerminatorIsBranch(llvm::BasicBlock *bb) {
  auto inst = bb->getTerminator();
  if (inst) {
    llvm::BranchInst *branchInst = llvm::dyn_cast<llvm::BranchInst>(inst);
    if (branchInst && !branchInst->isConditional()) {
      return true;
    }
  }
  return false;
}

int getActualFramesize(tr::Level *level) {
  return level->frame_->calculateActualFramesize();
}

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

class ValAndTy {
public:
  type::Ty *ty_;
  llvm::Value *val_;

  ValAndTy(llvm::Value *val, type::Ty *ty) : val_(val), ty_(ty) {}
};

void ProgTr::OutputIR(std::string_view filename) {
  std::string llvmfile = std::string(filename) + ".ll";
  std::error_code ec;
  llvm::raw_fd_ostream out(llvmfile, ec, llvm::sys::fs::OpenFlags::OF_Text);
  ir_module->print(out, nullptr);
}

void ProgTr::Translate() {
  FillBaseVEnv();
  FillBaseTEnv();

  frame_info.push_back(std::make_pair("tigermain", main_level_->frame_));

  // declare i64 @init_array(i32, i64)
  llvm::FunctionType *init_array_type = llvm::FunctionType::get(
      ir_builder->getInt64Ty(),
      {ir_builder->getInt32Ty(), ir_builder->getInt32Ty()}, false);
  init_array =
      llvm::Function::Create(init_array_type, llvm::Function::ExternalLinkage,
                             "init_array", ir_module);
  // declare i1 @string_equal(% string *, % string *)
  llvm::FunctionType *string_equal_type =
      llvm::FunctionType::get(ir_builder->getInt1Ty(),
                              {type::StringTy::Instance()->GetLLVMType(),
                               type::StringTy::Instance()->GetLLVMType()},
                              false);
  string_equal =
      llvm::Function::Create(string_equal_type, llvm::Function::ExternalLinkage,
                             "string_equal", ir_module);
  // declare i64 @alloc_record(i32)
  llvm::FunctionType *alloc_record_type = llvm::FunctionType::get(
      ir_builder->getInt64Ty(), {ir_builder->getInt32Ty()}, false);
  alloc_record =
      llvm::Function::Create(alloc_record_type, llvm::Function::ExternalLinkage,
                             "alloc_record", ir_module);

  std::vector<llvm::Type *> formals_ty;
  formals_ty.push_back(ir_builder->getInt64Ty());
  formals_ty.push_back(ir_builder->getInt64Ty());
  llvm::FunctionType *func_type =
      llvm::FunctionType::get(ir_builder->getInt32Ty(), formals_ty, false);
  llvm::Function *func = llvm::Function::Create(
      func_type, llvm::Function::ExternalLinkage, "tigermain", ir_module);

  std::cout << "start translate" << std::endl;
  llvm::GlobalVariable *global_frame_size = new llvm::GlobalVariable(
      llvm::Type::getInt64Ty(ir_builder->getContext()), true,
      llvm::GlobalValue::InternalLinkage,
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_builder->getContext()),
                             0),
      "tigermain_framesize_global");
  ir_module->getGlobalList().push_back(global_frame_size);
  func_stack.push(func);
  auto block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "tigermain", func);
  ir_builder->SetInsertPoint(block);

  llvm::Function::arg_iterator real_arg_it = func->arg_begin();
  llvm::Value *function_sp = ir_builder->CreateAdd(
      real_arg_it, llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0),
      "tigermain_sp");
  main_level_->set_sp(function_sp);
  main_level_->frame_->framesize_global = global_frame_size;

  auto main_res = this->absyn_tree_->Translate(
      venv_.get(), tenv_.get(), main_level_.get(), errormsg_.get());
  global_frame_size->setInitializer(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_builder->getContext()),
                             main_level_->frame_->calculateActualFramesize()));
  if (main_res->ty_ != type::VoidTy::Instance())
    ir_builder->CreateRet(main_res->val_);
  else
    ir_builder->CreateRet(llvm::ConstantInt::get(ir_builder->getInt32Ty(), 0));
  func_stack.pop();

  /* TODO: Put your lab5-part1 code here */
}

} // namespace tr

namespace absyn {

tr::ValAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  std::cout << "AbsynTree translate" << std::endl;
  return this->root_->Translate(venv, tenv, level, errormsg);
  // done
  /* TODO: Put your lab5-part1 code here */
}

void TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                        err::ErrorMsg *errormsg) const {
  std::cout << "TypeDec Translate" << std::endl;
  auto &list = this->types_->GetList();

  for (auto &&it : list) {
    tenv->Enter(it->name_, new type::NameTy(it->name_, NULL));
  }
  for (auto &&it : list) {
    std::cout << typeid(*it->ty_).name() << std::endl;
    type::NameTy *name_ty = (type::NameTy *)tenv->Look(it->name_);
    type::Ty *ty = it->ty_->Translate(tenv, errormsg);
    name_ty->ty_ = ty;
  }
  // to be checked
  /* TODO: Put your lab5-part1 code here */
}

void FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, err::ErrorMsg *errormsg) const {
  std::cout << "FunctionDec Translate" << std::endl;
  // first loop create function entry for nest declaration
  for (auto &&func : this->functions_->GetList()) {
    // Create new frame and level
    auto label = temp::LabelFactory::NamedLabel(func->name_->Name());
    type::Ty *result_ty;
    if (func->result_)
      result_ty = tenv->Look(func->result_);
    else
      result_ty = type::VoidTy::Instance();

    auto frame = frame::NewFrame(
        label, std::list<bool>(func->params_->GetList().size() + 1, true));
    frame_info.push_back({func->name_->Name(), frame});
    tr::Level *new_level = new tr::Level(frame, level, level->number + 1);

    // Create new Function Entry and Enter to venv
    auto formal_list = func->params_->MakeFormalTyList(tenv, errormsg);

    std::vector<llvm::Type *> formals_ty;
    formals_ty.push_back(ir_builder->getInt64Ty()); // sp
    formals_ty.push_back(ir_builder->getInt64Ty()); // sl
    for (auto arg_ty : formal_list->GetList()) {
      formals_ty.push_back(arg_ty->GetLLVMType());
    }
    llvm::FunctionType *func_type =
        llvm::FunctionType::get(result_ty->GetLLVMType(), formals_ty, false);
    llvm::Function *function =
        llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                               func->name_->Name(), ir_module);
    env::FunEntry *fun_entry = new env::FunEntry(
        new_level, formal_list, result_ty, func_type, function);

    venv->Enter(func->name_, fun_entry);
  }
  auto origin_block = ir_builder->GetInsertBlock();
  std::cout << "finish first passing" << std::endl;
  // now start the translation for passing arg and body
  for (auto &&func : this->functions_->GetList()) {
    venv->BeginScope();
    tenv->BeginScope();
    // create global variable for storing frame-size
    llvm::GlobalVariable *global_frame_size = new llvm::GlobalVariable(
        llvm::Type::getInt64Ty(ir_builder->getContext()), false,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ir_builder->getContext()),
                               0),
        func->name_->Name() + "_framesize_global");

    ir_module->getGlobalList().push_back(global_frame_size);

    // set global variable to frame
    auto entry = venv->Look(func->name_);
    auto fun_entry = dynamic_cast<env::FunEntry *>(entry);
    fun_entry->level_->frame_->framesize_global = global_frame_size;

    // begin body translation
    ir_builder->SetInsertPoint(llvm::BasicBlock::Create(
        ir_builder->getContext(), func->name_->Name(), fun_entry->func_));
    func_stack.push(fun_entry->func_);

    std::cout << "start passing params" << std::endl;
    // pass params first
    auto formal_it = func->params_->GetList().begin();
    auto args_it =
        ++fun_entry->level_->frame_->formals_->begin(); // skip static link
    llvm::Function::arg_iterator real_arg_it = fun_entry->func_->arg_begin();

    // real_arg1: sp
    llvm::Value *function_sp = ir_builder->CreateAdd(
        real_arg_it, llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0),
        func->name_->Name() + "_sp");
    fun_entry->level_->set_sp(function_sp);
    real_arg_it++;
    std::cout << "set sp" << std::endl;
    // real_arg2: sl
    (*fun_entry->level_->frame_->formals_->begin())
        ->setValue(real_arg_it++, ir_builder->getInt64Ty());
    std::cout << "start passing user defined args " << std::endl;
    // true_arg (user defined)
    for (; formal_it != func->params_->GetList().end();
         formal_it++, args_it++) {
      // formal type
      std::cout << (*formal_it)->typ_->Name() << "formal type" << std::endl;
      type::Ty *ty_ = tenv->Look((*formal_it)->typ_);
      tr::Access *arg_access = new tr::Access(fun_entry->level_, (*args_it));
      arg_access->access_->setValue(real_arg_it++, ty_->GetLLVMType());
      venv->Enter((*formal_it)->name_,
                  new env::VarEntry(arg_access, ty_->ActualTy()));
      std::cout << (*formal_it)->name_->Name() << ": "
                << typeid(ty_->ActualTy()).name() << " == int ? "
                << (ty_->IsSameType(type::IntTy::Instance())) << std::endl;
    }
    std::cout << "body translate" << std::endl;
    auto result_val_ty =
        func->body_->Translate(venv, tenv, fun_entry->level_, errormsg);
    if (fun_entry->result_ != type::VoidTy::Instance()) {
      if (result_val_ty->val_ &&
          fun_entry->func_->getReturnType() != result_val_ty->val_->getType() &&
          result_val_ty->ty_->IsSameType(type::IntTy::Instance())) {
        auto ret_val = ir_builder->CreateZExt(
            result_val_ty->val_, fun_entry->func_->getReturnType());
        ir_builder->CreateRet(ret_val);
      } else
        ir_builder->CreateRet(result_val_ty->val_);
    } else
      ir_builder->CreateRetVoid();
    auto real_size = fun_entry->level_->frame_->calculateActualFramesize();

    // needs 16 align?
    // set back to global frame size
    global_frame_size->setInitializer(llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(ir_builder->getContext()), real_size));

    std::cout << func->name_->Name() + "_frame_size: " << real_size
              << std::endl;
    std::cout << "outgo space: " << fun_entry->level_->frame_->outgo_size_
              << std::endl;
    std::cout << "offset: " << fun_entry->level_->frame_->offset_ << std::endl;
    func_stack.pop();
    venv->EndScope();
    tenv->EndScope();
  }
  ir_builder->SetInsertPoint(origin_block);
  /* TODO: Put your lab5-part1 code here */
}

void VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv, tr::Level *level,
                       err::ErrorMsg *errormsg) const {

  std::cout << "varDec Translate" << std::endl;
  auto var_access = tr::Access::AllocLocal(level, this->escape_);
  auto init_val_ty = this->init_->Translate(venv, tenv, level, errormsg);
  var_access->access_->setValue(init_val_ty->val_,
                                init_val_ty->ty_->GetLLVMType());
  venv->Enter(this->var_,
              new env::VarEntry(var_access, init_val_ty->ty_->ActualTy()));
  // done
  /* TODO: Put your lab5-part1 code here */
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  std::cout << "NameTy Translate" << std::endl;
  auto ty_ = tenv->Look(this->name_);
  assert(ty_);
  return new type::NameTy(this->name_, ty_);
  // to be checked
  /* TODO: Put your lab5-part1 code here */
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  std::cout << "RecordTy Translate" << std::endl;
  auto list = this->record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(list);
  /* TODO: Put your lab5-part1 code here */
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  std::cout << "ArrayTy Translate" << std::endl;
  return new type::ArrayTy(tenv->Look(this->array_)->ActualTy());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  std::cout << "SimpleVar Translate" << std::endl;
  auto var = venv->Look(this->sym_);
  auto simple_var = dynamic_cast<env::VarEntry *>(var);
  auto access = simple_var->access_;

  if (access->level_ != level) {
    // access by trace static link
    std::cout << "use static link" << std::endl;
    llvm::Value *val = level->get_sp();
    while (level != access->level_) {
      auto sl_formal = level->frame_->Formals()->begin();
      llvm::Value *sl_addr = (*sl_formal)->GetAccess(val);
      llvm::Value *sl_ptr = ir_builder->CreateIntToPtr(
          sl_addr, llvm::Type::getInt64PtrTy(ir_builder->getContext()));
      val = ir_builder->CreateLoad(ir_builder->getInt64Ty(), sl_ptr);
      level = level->parent_;
    }
    auto addr_int = access->access_->GetAccess(val);
    auto addr_ptr = ir_builder->CreateIntToPtr(
        addr_int, llvm::PointerType::get(simple_var->ty_->GetLLVMType(), 0));
    return new tr::ValAndTy(addr_ptr, simple_var->ty_->ActualTy());
  } else {
    // directly access
    std::cout << "direct access" << std::endl;
    std::cout << "int ? : "
              << simple_var->ty_->IsSameType(type::IntTy::Instance())
              << std::endl;
    auto var_ptr = ir_builder->CreateIntToPtr(
        access->access_->GetAccess(),
        llvm::PointerType::get(simple_var->ty_->GetLLVMType(), 0));
    return new tr::ValAndTy(var_ptr, simple_var->ty_->ActualTy());
  }
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  std::cout << "FeildVar Translate" << std::endl;
  auto val_ty = this->var_->Translate(venv, tenv, level, errormsg);
  auto record_ty = dynamic_cast<type::RecordTy *>(val_ty->ty_->ActualTy());
  auto struct_ptr =
      ir_builder->CreateLoad(val_ty->ty_->GetLLVMType(), val_ty->val_);
  int index = 0;

  for (auto &&field : record_ty->fields_->GetList()) {
    if (field->name_ == this->sym_) {
      auto llvm_val = ir_builder->CreateGEP(
          val_ty->ty_->GetLLVMType()->getPointerElementType(), struct_ptr,
          {llvm::ConstantInt::get(ir_builder->getInt32Ty(), 0),
           llvm::ConstantInt::get(ir_builder->getInt32Ty(), index)});
      return new tr::ValAndTy(llvm_val, field->ty_);
    }
    index++;
  }
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level,
                                      err::ErrorMsg *errormsg) const {
  std::cout << "SubScriptVar Translate" << std::endl;
  auto var_val_ty = this->var_->Translate(venv, tenv, level, errormsg);
  auto array_ptr =
      ir_builder->CreateLoad(var_val_ty->ty_->GetLLVMType(), var_val_ty->val_);

  auto subscript_val_ty =
      this->subscript_->Translate(venv, tenv, level, errormsg);

  llvm::Value *subscript_val = subscript_val_ty->val_;
  auto llvm_val = ir_builder->CreateGEP(
      var_val_ty->ty_->GetLLVMType()->getPointerElementType(), array_ptr,
      subscript_val);

  auto arrayTy = static_cast<type::ArrayTy *>(var_val_ty->ty_);
  return new tr::ValAndTy(llvm_val, arrayTy->ty_->ActualTy());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  std::cout << "VarExp Translate" << std::endl;
  auto var_res = this->var_->Translate(venv, tenv, level, errormsg);
  std::cout << var_res->ty_->GetLLVMType() << "\n" << var_res->val_ << "\n";
  std::cout << "get var type "
            << (typeid(var_res->ty_->ActualTy()) ==
                typeid(type::IntTy::Instance))
            << std::endl;
  // std::string outfile =
  //     static_cast<std::string>(
  //         "/home/stu/tiger-compiler/testdata/lab5or6/testcases/demo.tig") +
  //     ".ll";
  // std::error_code EC;
  // llvm::raw_fd_ostream file(outfile, EC);
  // ir_module->print(file, nullptr);
  auto val = ir_builder->CreateLoad(var_res->ty_->GetLLVMType(), var_res->val_);
  std::cout << "varexp done" << std::endl;
  return new tr::ValAndTy(val, var_res->ty_->ActualTy());
  // done
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  std::cout << "NilExp Translate" << std::endl;
  return new tr::ValAndTy(nullptr, type::NilTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  std::cout << "IntExp Translate" << std::endl;
  return new tr::ValAndTy(
      llvm::ConstantInt::get(ir_builder->getInt32Ty(), this->val_, true),
      type::IntTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  std::cout << "StringExp Translate" << std::endl;
  return new tr::ValAndTy(
      type::StringTy::CreateGlobalStringStructPtr(this->str_),
      type::StringTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  std::cout << "CallExp Translate" << std::endl;
  auto entry = venv->Look(this->func_);
  auto fun_entry = dynamic_cast<env::FunEntry *>(entry);
  std::cout << "found function entry" << std::endl;
  auto args_num = fun_entry->formals_->GetList().size();
  std::vector<llvm::Value *> formal_args;
  std::vector<llvm::Value *> real_args;

  for (auto &&arg_it : this->args_->GetList()) {
    std::cout << "get arg" << std::endl;
    auto res = arg_it->Translate(venv, tenv, level, errormsg);
    formal_args.emplace_back(res->val_);
  }
  std::cout << "finish get formal args" << std::endl;

  if (fun_entry->level_->parent_ == nullptr) {
    std::cout << "lib function" << std ::endl;
    level->frame_->AllocOutgoSpace((args_num)*reg_manager->WordSize());
    real_args = formal_args;
  } else {
    std::cout << "allocOutgoSpace: " << (args_num + 1) * reg_manager->WordSize()
              << std::endl;
    level->frame_->AllocOutgoSpace((args_num + 1) * reg_manager->WordSize());

    int callee_level = fun_entry->level_->number;
    int curr_level = level->number;
    if (callee_level <= curr_level) {
      auto trace_level = level;
      llvm::Value *trace_sl_int = level->get_sp();
      while (curr_level != callee_level + 1) {
        auto sl_formal = trace_level->frame_->Formals()->begin();
        llvm::Value *sl_int = (*sl_formal)->GetAccess(trace_sl_int);
        llvm::Value *sl_ptr = ir_builder->CreateIntToPtr(
            sl_int, llvm::Type::getInt64PtrTy(ir_builder->getContext()));
        trace_sl_int = ir_builder->CreateLoad(ir_builder->getInt64Ty(), sl_ptr);
        trace_level = trace_level->parent_;
        curr_level++;
      }
      real_args.emplace_back(trace_sl_int);
    } else {
      assert(callee_level == curr_level + 1);
      // real_args.emplace_back(level->get_sp());
      // modify in lab5-part2, because of the shit code design
      auto sl_value = ir_builder->CreateAdd(
          level->get_sp(), llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0));
      real_args.emplace_back(sl_value);
    }

    real_args.insert(real_args.end(), formal_args.begin(), formal_args.end());

    // auto global_framesize_val = ir_builder->CreateLoad(
    //     ir_builder->getInt64Ty(), level->frame_->framesize_global);
    auto global_framesize_val = ir_builder->CreateLoad(
        ir_builder->getInt64Ty(), level->frame_->framesize_global);
    auto new_sp = ir_builder->CreateSub(level->get_sp(), global_framesize_val);

    real_args.insert(real_args.begin(), new_sp);
  }

  auto callInst = ir_builder->CreateCall(fun_entry->func_, real_args);
  if (fun_entry->result_ == type::VoidTy::Instance())
    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  return new tr::ValAndTy(callInst, fun_entry->result_);
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  std::cout << "OpExp Translate" << std::endl;
  auto left_val_ty = this->left_->Translate(venv, tenv, level, errormsg);
  auto right_val_ty = this->right_->Translate(venv, tenv, level, errormsg);

  if (left_val_ty->ty_->ActualTy() == type::StringTy::Instance() &&
      right_val_ty->ty_->ActualTy() == type::StringTy::Instance()) {
    llvm::Value *retVal;
    auto res = ir_builder->CreateCall(string_equal,
                                      {left_val_ty->val_, right_val_ty->val_});
    if (this->oper_ == NEQ_OP) {
      retVal = ir_builder->CreateICmpNE(
          res, llvm::ConstantInt::get(
                   llvm::Type::getInt1Ty(ir_builder->getContext()), 1));
    } else {
      retVal = ir_builder->CreateICmpEQ(
          res, llvm::ConstantInt::get(
                   llvm::Type::getInt1Ty(ir_builder->getContext()), 1));
    }
    return new tr::ValAndTy(retVal, type::IntTy::Instance());
  }

  if (left_val_ty->ty_ == type::NilTy::Instance() ||
      right_val_ty->ty_ == type::NilTy::Instance()) {
    if (left_val_ty->ty_ == type::NilTy::Instance() &&
        right_val_ty->ty_ == type::NilTy::Instance()) {
      if (this->oper_ == EQ_OP) {
        return new tr::ValAndTy(
            llvm::ConstantInt::get(ir_builder->getInt1Ty(), 1),
            type::IntTy::Instance());
      } else if (this->oper_ == NEQ_OP) {
        return new tr::ValAndTy(
            llvm::ConstantInt::get(ir_builder->getInt1Ty(), 0),
            type::IntTy::Instance());
      }
    }
    llvm::Value *res;
    llvm::Value *val;
    if (left_val_ty->ty_ == type::NilTy::Instance()) {
      val = ir_builder->CreatePtrToInt(right_val_ty->val_,
                                       ir_builder->getInt64Ty());
    } else {
      val = ir_builder->CreatePtrToInt(left_val_ty->val_,
                                       ir_builder->getInt64Ty());
    }
    if (this->oper_ == EQ_OP) {
      res = ir_builder->CreateICmpEQ(
          val, llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0));
    } else if (this->oper_ == NEQ_OP) {
      res = ir_builder->CreateICmpNE(
          val, llvm::ConstantInt::get(ir_builder->getInt64Ty(), 0));
    }
    return new tr::ValAndTy(res, type::IntTy::Instance());
  }

  llvm::Value *left_val, *right_val;
  left_val = left_val_ty->val_;
  right_val = right_val_ty->val_;
  llvm::Value *res;

  llvm::Function *curr_func = func_stack.top();
  if (this->oper_ == AND_OP) {
    auto and_left_test_bb = llvm::BasicBlock::Create(
        ir_builder->getContext(), "and_left_test", curr_func);
    auto and_right_test_bb = llvm::BasicBlock::Create(
        ir_builder->getContext(), "and_right_test", curr_func);
    auto and_next_bb = llvm::BasicBlock::Create(ir_builder->getContext(),
                                                "and_next", curr_func);
    ir_builder->CreateBr(and_left_test_bb);
    ir_builder->SetInsertPoint(and_left_test_bb);
    auto left_val_int = ir_builder->CreateICmpNE(
        left_val, llvm::ConstantInt::get(left_val->getType(), 0));
    ir_builder->CreateCondBr(left_val_int, and_right_test_bb, and_next_bb);

    ir_builder->SetInsertPoint(and_right_test_bb);
    auto right_val_int = ir_builder->CreateICmpNE(
        right_val, llvm::ConstantInt::get(right_val->getType(), 0));
    ir_builder->CreateBr(and_next_bb);

    ir_builder->SetInsertPoint(and_next_bb);
    llvm::PHINode *and_phi = ir_builder->CreatePHI(ir_builder->getInt1Ty(), 2);
    and_phi->addIncoming(
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(ir_builder->getContext()),
                               0),
        and_left_test_bb);
    and_phi->addIncoming(right_val_int, and_right_test_bb);
    return new tr::ValAndTy(and_phi, type::IntTy::Instance());
  } else if (this->oper_ == OR_OP) {
    auto left_test_bb = llvm::BasicBlock::Create(ir_builder->getContext(),
                                                 "left_test", curr_func);
    auto right_test_bb = llvm::BasicBlock::Create(ir_builder->getContext(),
                                                  "right_test", curr_func);
    auto next_bb = llvm::BasicBlock::Create(ir_builder->getContext(),
                                            "next_block", curr_func);
    ir_builder->CreateBr(left_test_bb);
    ir_builder->SetInsertPoint(left_test_bb);
    auto left_val_int = ir_builder->CreateICmpNE(
        left_val, llvm::ConstantInt::get(left_val->getType(), 0));
    ir_builder->CreateCondBr(left_val_int, next_bb, right_test_bb);

    ir_builder->SetInsertPoint(right_test_bb);
    auto right_val_int = ir_builder->CreateICmpNE(
        right_val, llvm::ConstantInt::get(right_val->getType(), 0));
    ir_builder->CreateBr(next_bb);

    ir_builder->SetInsertPoint(next_bb);
    llvm::PHINode *phi = ir_builder->CreatePHI(ir_builder->getInt1Ty(), 2);
    phi->addIncoming(llvm::ConstantInt::get(
                         llvm::Type::getInt1Ty(ir_builder->getContext()), 1),
                     left_test_bb);
    phi->addIncoming(right_val_int, right_test_bb);
    return new tr::ValAndTy(phi, type::IntTy::Instance());
  }
  switch (this->oper_) {
  case EQ_OP:
    res = ir_builder->CreateICmpEQ(left_val, right_val);
    break;
  case NEQ_OP:
    res = ir_builder->CreateICmpNE(left_val, right_val);
    break;
  case LT_OP:
    res = ir_builder->CreateICmpSLT(left_val, right_val);
    break;
  case LE_OP:
    res = ir_builder->CreateICmpSLE(left_val, right_val);
    break;
  case GT_OP:
    res = ir_builder->CreateICmpSGT(left_val, right_val);
    break;
  case GE_OP:
    res = ir_builder->CreateICmpSGE(left_val, right_val);
    break;
  case PLUS_OP:
    res = ir_builder->CreateAdd(left_val, right_val);
    break;
  case MINUS_OP:
    res = ir_builder->CreateSub(left_val, right_val);
    break;
  case TIMES_OP:
    res = ir_builder->CreateMul(left_val, right_val);
    break;
  case DIVIDE_OP:
    res = ir_builder->CreateSDiv(left_val, right_val);
    break;
  default:
    assert(0);
  }

  return new tr::ValAndTy(res, type::IntTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  std::cout << "RecordExp Translate" << std::endl;
  std::vector<llvm::Value *> args;
  auto record_ty =
      static_cast<type::RecordTy *>(tenv->Look(this->typ_)->ActualTy());
  int field_num = record_ty->fields_->GetList().size();
  int space_needed = field_num * reg_manager->WordSize();
  auto alloc_res = ir_builder->CreateCall(
      alloc_record,
      {llvm::ConstantInt::get(ir_builder->getInt32Ty(), space_needed)});
  auto record_ptr =
      ir_builder->CreateIntToPtr(alloc_res, record_ty->GetLLVMType());

  int index = 0;
  for (auto &&feild : this->fields_->GetList()) {
    auto feild_val_ty = feild->exp_->Translate(venv, tenv, level, errormsg);
    auto feild_ptr = ir_builder->CreateGEP(
        record_ty->GetLLVMType()->getPointerElementType(), record_ptr,
        {llvm::ConstantInt::get(ir_builder->getInt32Ty(), 0),
         llvm::ConstantInt::get(ir_builder->getInt32Ty(), index++)});
    ir_builder->CreateStore(feild_val_ty->val_, feild_ptr);
  }

  return new tr::ValAndTy(record_ptr, record_ty->ActualTy());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  std::cout << "SeqExp Translate" << std::endl;
  tr::ValAndTy *res;
  for (auto &&exp : this->seq_->GetList()) {
    res = exp->Translate(venv, tenv, level, errormsg);
  }
  return res;
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level,
                                   err::ErrorMsg *errormsg) const {
  std::cout << "AssignExp Translate" << std::endl;
  auto exp_val_ty = this->exp_->Translate(venv, tenv, level, errormsg);
  auto var_val_ty = this->var_->Translate(venv, tenv, level, errormsg);

  ir_builder->CreateStore(exp_val_ty->val_, var_val_ty->val_);
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level,
                               err::ErrorMsg *errormsg) const {
  std::cout << "IfExp Translate" << std::endl;

  llvm::Function *curr_func = func_stack.top();
  auto then_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "then", curr_func);
  auto else_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "else", curr_func);
  auto next_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "next", curr_func);

  auto test_res = this->test_->Translate(venv, tenv, level, errormsg);
  llvm::Value *test_val = ir_builder->CreateICmpNE(
      test_res->val_, llvm::ConstantInt::get(test_res->val_->getType(), 0));
  ir_builder->CreateCondBr(test_val, then_block, else_block);

  // then block
  ir_builder->SetInsertPoint(then_block);
  auto then_res = this->then_->Translate(venv, tenv, level, errormsg);
  auto then_last_block = ir_builder->GetInsertBlock();
  if (!CheckBBTerminatorIsBranch(then_last_block))
    ir_builder->CreateBr(next_block);

  // when no elsee
  if (!this->elsee_) {
    ir_builder->SetInsertPoint(else_block);
    ir_builder->CreateBr(next_block);
    ir_builder->SetInsertPoint(next_block);
    return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  }

  // else block
  ir_builder->SetInsertPoint(else_block);
  auto else_res = this->elsee_->Translate(venv, tenv, level, errormsg);
  auto else_last_block = ir_builder->GetInsertBlock();
  if (!CheckBBTerminatorIsBranch(else_last_block))
    ir_builder->CreateBr(next_block);

  ir_builder->SetInsertPoint(next_block);
  if (then_res->val_ && else_res->val_) {
    auto phi = ir_builder->CreatePHI(then_res->ty_->GetLLVMType(), 2);
    phi->addIncoming(then_res->val_, then_last_block);
    phi->addIncoming(else_res->val_, else_last_block);

    return new tr::ValAndTy(phi, else_res->ty_);
  } else if (then_res->ty_->IsSameType(else_res->ty_)) {
    if (!then_res->val_ && !else_res->val_) {
      return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
    }
    auto phi = ir_builder->CreatePHI(then_res->ty_->GetLLVMType(), 2);
    if (!then_res->val_) {
      auto rec_ptr_ty =
          llvm::dyn_cast<llvm::PointerType>(else_res->ty_->GetLLVMType());
      auto null_val = llvm::ConstantPointerNull::get(rec_ptr_ty);
      phi->addIncoming(null_val, then_last_block);
    } else {
      phi->addIncoming(then_res->val_, then_last_block);
    }
    if (!else_res->val_) {
      auto rec_ptr_ty =
          llvm::dyn_cast<llvm::PointerType>(then_res->ty_->GetLLVMType());
      auto null_val = llvm::ConstantPointerNull::get(rec_ptr_ty);
      phi->addIncoming(null_val, else_last_block);
    } else {
      phi->addIncoming(else_res->val_, else_last_block);
    }
    return new tr::ValAndTy(phi, then_res->ty_);
  }
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());

  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  std::cout << "WhileExp Translate" << std::endl;
  llvm::Function *curr_func = func_stack.top();
  llvm::BasicBlock *test_block = llvm::BasicBlock::Create(
      ir_builder->getContext(), "while_test", curr_func);
  llvm::BasicBlock *done_block = llvm::BasicBlock::Create(
      ir_builder->getContext(), "while_done", curr_func);
  llvm::BasicBlock *body_block = llvm::BasicBlock::Create(
      ir_builder->getContext(), "while_body", curr_func);

  ir_builder->CreateBr(test_block);
  ir_builder->SetInsertPoint(test_block);
  auto test_val_ty = this->test_->Translate(venv, tenv, level, errormsg);
  llvm::Value *cond_val = ir_builder->CreateICmpNE(
      test_val_ty->val_,
      llvm::ConstantInt::get(test_val_ty->val_->getType(), 0));
  ir_builder->CreateCondBr(cond_val, body_block, done_block);

  ir_builder->SetInsertPoint(body_block);
  loop_stack.emplace(done_block);
  this->body_->Translate(venv, tenv, level, errormsg);
  loop_stack.pop();
  ir_builder->CreateBr(test_block);

  ir_builder->SetInsertPoint(done_block);
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  std::cout << "ForExp Translate" << std::endl;
  auto loop_var = tr::Access::AllocLocal(level, this->escape_);
  auto lo_val_ty = this->lo_->Translate(venv, tenv, level, errormsg);
  auto hi_val_ty = this->hi_->Translate(venv, tenv, level, errormsg);

  loop_var->access_->setValue(lo_val_ty->val_, lo_val_ty->ty_->GetLLVMType());
  venv->BeginScope();
  venv->Enter(this->var_,
              new env::VarEntry(loop_var, type::IntTy::Instance(), true));
  llvm::Function *curr_func = func_stack.top();
  llvm::BasicBlock *test_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "for_test", curr_func);
  llvm::BasicBlock *done_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "for_done", curr_func);
  llvm::BasicBlock *body_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "for_body", curr_func);
  llvm::BasicBlock *loop_end_block =
      llvm::BasicBlock::Create(ir_builder->getContext(), "for_end", curr_func);

  ir_builder->CreateBr(test_block);
  ir_builder->SetInsertPoint(test_block);
  auto loopvar_int = loop_var->access_->GetAccess();
  auto loopvar_ptr = ir_builder->CreateIntToPtr(
      loopvar_int, llvm::PointerType::get(lo_val_ty->ty_->GetLLVMType(), 0));
  auto loop_var_val =
      ir_builder->CreateLoad(lo_val_ty->ty_->GetLLVMType(), loopvar_ptr);
  ir_builder->CreateCondBr(
      ir_builder->CreateICmpSLE(loop_var_val, hi_val_ty->val_), body_block,
      done_block);

  ir_builder->SetInsertPoint(body_block);
  loop_stack.emplace(done_block);
  this->body_->Translate(venv, tenv, level, errormsg);
  loop_stack.pop();
  ir_builder->CreateBr(loop_end_block);

  ir_builder->SetInsertPoint(loop_end_block);
  auto next_val = ir_builder->CreateAdd(
      loop_var_val, llvm::ConstantInt::get(ir_builder->getInt32Ty(), 1));
  loop_var->access_->setValue(next_val, lo_val_ty->ty_->GetLLVMType());
  ir_builder->CreateBr(test_block);

  ir_builder->SetInsertPoint(done_block);
  venv->EndScope();
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  std::cout << "BreakExp Translate" << std::endl;
  ir_builder->CreateBr(loop_stack.top());
  loop_stack.pop();
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level,
                                err::ErrorMsg *errormsg) const {
  std::cout << "LetExp Translate" << std::endl;
  venv->BeginScope();
  tenv->BeginScope();
  for (auto &&dec : this->decs_->GetList()) {
    dec->Translate(venv, tenv, level, errormsg);
  }
  auto val = this->body_->Translate(venv, tenv, level, errormsg);
  venv->EndScope();
  tenv->EndScope();
  std::cout << "let exp done" << std::endl;
  return val;
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level,
                                  err::ErrorMsg *errormsg) const {
  std::cout << "ArrayExp Translate" << std::endl;
  auto size_val_ty = this->size_->Translate(venv, tenv, level, errormsg);
  auto init_val_ty = this->init_->Translate(venv, tenv, level, errormsg);
  auto ty = tenv->Look(this->typ_);
  llvm::Value *init_val = init_val_ty->val_;

  llvm::Value *size_val = size_val_ty->val_;
  auto init_res = ir_builder->CreateCall(init_array, {size_val, init_val});
  auto array_ptr = ir_builder->CreateIntToPtr(init_res, ty->GetLLVMType());
  std::cout << "array exp done" << std::endl;
  return new tr::ValAndTy(array_ptr, ty->ActualTy());
  /* TODO: Put your lab5-part1 code here */
}

tr::ValAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level,
                                 err::ErrorMsg *errormsg) const {
  std::cout << "VoidExp Translate" << std::endl;
  return new tr::ValAndTy(nullptr, type::VoidTy::Instance());
  /* TODO: Put your lab5-part1 code here */
}

} // namespace absyn