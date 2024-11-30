#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"
#include <iostream>

namespace absyn {
void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  this->root_->SemAnalyze(venv, tenv, 0, errormsg);
  /* TODO: Put your lab4 code here */
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  // Look up the variable in the venv, make sure it's predefined
  std::cout << "SimpleVar::SemAnalyze" << std::endl;
  env::EnvEntry *entry = venv->Look(this->sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return static_cast<env::VarEntry *>(entry)->ty_->ActualTy();
  } else {
    errormsg->Error(this->pos_, "undefined variable %s",
                    this->sym_->Name().c_str());
    return type::IntTy::Instance();
  }
  /* TODO: Put your lab4 code here */
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "FieldVar::SemAnalyze" << std::endl;
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(this->pos_, "not a record type");
    return type::IntTy::Instance();
  }
  auto &fields = static_cast<type::RecordTy *>(ty)->fields_->GetList();
  for (auto &&it : fields) {
    if (it->name_ == sym_) {
      return it->ty_->ActualTy();
    }
  }
  errormsg->Error(this->pos_, "field %s doesn't exist", sym_->Name().c_str());
  return type::IntTy::Instance();
  /* TODO: Put your lab4 code here */
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  std::cout << "SubscriptVar::SemAnalyze" << std::endl;
  type::Ty *varType = this->var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*varType) != typeid(type::ArrayTy)) {
    errormsg->Error(this->pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *subscriptType =
      this->subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*subscriptType) != typeid(type::IntTy)) {
    errormsg->Error(this->pos_, "subscript must be an integer");
    return type::IntTy::Instance();
  }
  return static_cast<type::ArrayTy *>(varType)->ty_->ActualTy();
  /* TODO: Put your lab4 code here */
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  // warning: to be checked
  /* TODO: Put your lab4 code here */
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "nill type" << std::endl;
  return type::NilTy::Instance();
  /* TODO: Put your lab4 code here */
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  // warning: to be checked
  std::cout << "CallExp::SemAnalyze" << std::endl;
  env::EnvEntry *entry = venv->Look(this->func_);
  std::cout << "Look up the function" << std::endl;
  if (!entry) {
    errormsg->Error(this->pos_, "undefined function %s",
                    this->func_->Name().c_str());
    return type::IntTy::Instance();
  }
  if (typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(this->pos_, "%s is not a function",
                    this->func_->Name().c_str());
    return type::IntTy::Instance();
  }
  std::cout << "Checked the function" << std::endl;

  auto *funEntry = static_cast<env::FunEntry *>(entry);
  auto &funEntryList = funEntry->formals_->GetList();
  auto &actualList = this->args_->GetList();
  std::cout << funEntryList.size() << " " << actualList.size() << std::endl;
  if (funEntryList.size() < actualList.size()) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().c_str());
  }
  if (funEntryList.size() > actualList.size()) {
    errormsg->Error(pos_, "too little params in function %s",
                    func_->Name().c_str());
  };
  auto formalIt = funEntryList.begin();
  auto actualIt = actualList.begin();

  std::cout << "parameter match" << std::endl;
  for (; formalIt != funEntryList.end() && actualIt != actualList.end();
       ++formalIt, ++actualIt) {
    std::cout << "iter" << std::endl;

    if (!(*formalIt)->IsSameType(
            (*actualIt)->SemAnalyze(venv, tenv, labelcount, errormsg))) {
      errormsg->Error(this->pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
  }
  return funEntry->result_->ActualTy();
  /* TODO: Put your lab4 code here */
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "OpExp::SemAnalyze" << std::endl;
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (oper_ == Oper::PLUS_OP || oper_ == Oper::MINUS_OP ||
      oper_ == Oper::TIMES_OP || oper_ == Oper::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    } else if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
    return type::IntTy::Instance();
    /* TODO: Put your lab4 code here */
  } else {
    std::cout << "else" << std::endl;
    std::cout << left_ty->IsSameType(right_ty);
    std::cout << "next!" << std::endl;
    if (!left_ty->IsSameType(right_ty) ||
        typeid(*left_ty) == typeid(type::VoidTy)) {
      errormsg->Error(this->pos_, "same type required");
      return type::IntTy::Instance();
    }
    return type::IntTy::Instance();
  }
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "RecordExp::SemAnalyze" << std::endl;
  type::Ty *ty = tenv->Look(this->typ_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  return ty;
  // unimplemented
  /* TODO: Put your lab4 code here */
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "SeqExp::SemAnalyze" << std::endl;
  auto &list = this->seq_->GetList();
  type::Ty *ty;
  for (auto &&it : list) {
    ty = it->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return ty->ActualTy();
  /* TODO: Put your lab4 code here */
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "AssignExp::SemAnalyze" << std::endl;
  if (typeid(*var_) == typeid(SimpleVar)) {
    env::EnvEntry *entry = venv->Look(static_cast<SimpleVar *>(var_)->sym_);
    if (entry->readonly_) {
      errormsg->Error(this->pos_, "loop variable can't be assigned");
    }
  }
  type::Ty *varType = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *expType = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!varType->IsSameType(expType)) {
    errormsg->Error(this->pos_, "unmatched assign exp");
  }
  return type::VoidTy::Instance();
  /* TODO: Put your lab4 code here */
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "IfExp::SemAnalyze" << std::endl;
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(this->pos_, "integer required");
  }

  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (this->elsee_) {
    type::Ty *elsee_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!then_ty->IsSameType(elsee_ty)) {
      errormsg->Error(this->pos_, "then exp and else exp type mismatch");
      return type::IntTy::Instance();
    }
    return then_ty->ActualTy();
  } else {
    if (typeid(*then_ty) != typeid(type::VoidTy)) {
      errormsg->Error(this->pos_, "if-then exp's body must produce no value");
    }
    return type::VoidTy::Instance();
  }
  /* TODO: Put your lab4 code here */
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "WhileExp::SemAnalyze" << std::endl;
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(this->pos_, "integer required");
  }
  venv->BeginScope();
  tenv->BeginScope();
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(this->pos_, "while body must produce no value");
  }
  venv->EndScope();
  tenv->EndScope();
  return type::VoidTy::Instance();
  /* TODO: Put your lab4 code here */
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "ForExp::SemAnalyze" << std::endl;
  type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*lo_ty) != typeid(type::IntTy) ||
      typeid(*hi_ty) != typeid(type::IntTy)) {
    errormsg->Error(this->pos_, "for exp's range type is not integer");
    // warning: to be checked
  }
  venv->BeginScope();
  tenv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(this->pos_, "for body must produce no value");
  }
  venv->EndScope();
  tenv->EndScope();
  return type::VoidTy::Instance();
  /* TODO: Put your lab4 code here */
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
  /* TODO: Put your lab4 code here */
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "LetExp::SemAnalyze" << std::endl;
  venv->BeginScope();
  tenv->BeginScope();
  auto &decs = this->decs_->GetList();
  for (auto &&it : decs) {
    it->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  type::Ty *ty = this->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  venv->EndScope();
  tenv->EndScope();
  return ty;
  /* TODO: Put your lab4 code here */
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "ArrayExp::SemAnalyze" << std::endl;
  type::Ty *ty = tenv->Look(this->typ_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  type::Ty *size_ty = this->size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*size_ty) != typeid(type::IntTy)) {
    errormsg->Error(this->pos_, "array size must be an integer");
    return type::IntTy::Instance();
  }
  std::cout << typeid(*ty->ActualTy()).name() << std::endl;
  if (typeid(*ty->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(this->pos_, "not an array type");
    return type::IntTy::Instance();
  }
  type::Ty *init_ty = this->init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  std::cout << typeid(*init_ty).name() << std::endl;
  type::ArrayTy *array_ty = dynamic_cast<type::ArrayTy *>(ty->ActualTy());
  if (!array_ty->ty_->ActualTy()->IsSameType(init_ty)) {
    errormsg->Error(this->pos_, "type mismatch");
  }
  return ty->ActualTy();
  /* TODO: Put your lab4 code here */
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
  /* TODO: Put your lab4 code here */
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::cout << "FunctionDec::SemAnalyze" << std::endl;
  auto list = this->functions_->GetList();
  std::unordered_map<decltype((*list.begin())->name_->Name()), bool> map;
  for (auto &&it : list) {
    if (map.find(it->name_->Name()) != map.end()) {
      errormsg->Error(it->pos_, "two functions have the same name");
    }
    map[it->name_->Name()] = true;
    type::TyList *formals = it->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *result;
    if (it->result_) {
      result = tenv->Look(it->result_);
    } else {
      result = type::VoidTy::Instance();
    }

    if (!result) {
      errormsg->Error(it->pos_, "undefined result type %s",
                      it->result_->Name().c_str());
      result = type::IntTy::Instance();
      return;
    }
    std::cout << "result == void ? "
              << (typeid(*result) == typeid(type::VoidTy)) << std::endl;
    venv->Enter(it->name_, new env::FunEntry(formals, result));
  }
  std::cout << "functiondec: finish first loop" << std::endl;
  for (auto &&it : list) {
    venv->BeginScope();
    auto &fields = it->params_->GetList();
    for (auto &&field : fields) {
      type::Ty *ty = tenv->Look(field->typ_);
      if (!ty) {
        errormsg->Error(it->pos_, "undefined type %s",
                        field->typ_->Name().c_str());
        ty = type::IntTy::Instance();
      }
      venv->Enter(field->name_, new env::VarEntry(ty));
    }
    type::Ty *body_ty = it->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *result_ty =
        static_cast<env::FunEntry *>(venv->Look(it->name_))->result_;
    if (!body_ty->IsSameType(result_ty)) {
      if (typeid(*result_ty) == typeid(type::VoidTy))
        errormsg->Error(it->pos_, "procedure returns value");
      else
        errormsg->Error(it->pos_, "incorrect return type");
    }
    venv->EndScope();
  }
  /* TODO: Put your lab4 code here */
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  std::cout << "VarDec::SemAnalyze" << std::endl;
  if (typ_ == nullptr) {
    type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
    std::cout << typeid(*init_ty).name() << std::endl;
    std::cout << typeid(type::NilTy).name() << std::endl;
    if (typeid(*init_ty) == typeid(type::NilTy)) {
      errormsg->Error(this->pos_,
                      "init should not be nil without type specified");
    }
    venv->Enter(var_, new env::VarEntry(init_ty));
  } else {
    type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(this->pos_, "undefined type %s", typ_->Name().c_str());
      ty = type::IntTy::Instance();
    }
    std::cout << "init_ty: " << typeid(*init_ty).name() << std::endl;
    std::cout << "ty: " << typeid(*ty).name() << std::endl;

    if (!ty->IsSameType(init_ty)) {
      errormsg->Error(this->pos_, "type mismatch");
    } else if (typeid(*ty) == typeid(type::NameTy) &&
               typeid(*init_ty) == typeid(type::NameTy)) {
      if (static_cast<type::NameTy *>(ty)->sym_->Name() !=
          static_cast<type::NameTy *>(init_ty)->sym_->Name())
        errormsg->Error(this->pos_, "type mismatch");
    } else if (typeid(*ty) != typeid(*init_ty)) {
      errormsg->Error(this->pos_, "type mismatch");
    }
    venv->Enter(var_, new env::VarEntry(ty));
  }
  /* TODO: Put your lab4 code here */
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  std::cout << "TypeDec::SemAnalyze" << std::endl;
  auto &list = this->types_->GetList();

  std::unordered_map<decltype((*list.begin())->name_->Name()), bool> nameHash;
  for (auto &&it : list) {
    std::cout << "name: " << it->name_->Name() << std::endl;
    if (nameHash.find(it->name_->Name()) != nameHash.end()) {
      errormsg->Error(this->pos_, "two types have the same name");
      return;
    }
    nameHash[it->name_->Name()] = true;
    tenv->Enter(it->name_, new type::NameTy(it->name_, NULL));
  }
  for (auto &&it : list) {
    std::cout << typeid(*it->ty_).name() << std::endl;
    type::NameTy *name_ty = (type::NameTy *)tenv->Look(it->name_);
    type::Ty *ty = it->ty_->SemAnalyze(tenv, errormsg);
    if (!ty) {
      errormsg->Error(this->pos_, "undefined type %s",
                      it->name_->Name().c_str());
      ty = type::IntTy::Instance();
    }
    name_ty->ty_ = ty;
  }

  bool cycle = false;
  for (auto &&iter : list) {
    std::cout << "itername: " << iter->name_->Name() << std::endl;
    type::Ty *ty = tenv->Look(iter->name_);
    std::cout << "typeid: " << typeid(*ty).name() << std::endl;
    if (typeid(*ty) == typeid(type::NameTy)) {
      type::Ty *ty_ty = static_cast<type::NameTy *>(ty)->ty_;
      std::cout << "here" << std::endl;
      while (typeid(*ty_ty) == typeid(type::NameTy)) {
        type::NameTy *name_ty = static_cast<type::NameTy *>(ty_ty);
        std::cout << "ty_ty: " << name_ty->sym_->Name() << std::endl;
        if (name_ty->sym_->Name() == iter->name_->Name()) {
          errormsg->Error(pos_, "illegal type cycle");
          cycle = true;
          break;
        };
        ty_ty = name_ty->ty_;
      };
    }
    if (cycle)
      break;
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  std::cout << "NameTy::SemAnalyze" << std::endl;
  type::Ty *ty = tenv->Look(this->name_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->name_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::NameTy(this->name_, ty);
  /* TODO: Put your lab4 code here */
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  std::cout << "RecordTy::SemAnalyze" << std::endl;
  type::FieldList *fields = this->record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fields);
  /* TODO: Put your lab4 code here */
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  std::cout << "ArrayTy::SemAnalyze" << std::endl;
  type::Ty *ty = tenv->Look(this->array_);
  if (!ty) {
    errormsg->Error(this->pos_, "undefined type %s",
                    this->array_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
  /* TODO: Put your lab4 code here */
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
