#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int max1 = this->stm1->MaxArgs();
  int max2 = this->stm2->MaxArgs();
  return max1 >= max2 ? max1 : max2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *newTable = this->stm1->Interp(t);
  return this->stm2->Interp(newTable);
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return this->exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable result = this->exp->Interp(t);
  if (t == nullptr)
    return new Table(this->id, result.i, nullptr);
  else
    return t->Update(this->id, result.i);
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return this->exps->NumExps();
}

Table *A::PrintStm::Interp(Table *t) const {
  int numExps = this->exps->NumExps();
  return this->exps->Interp(t).t;
  // TODO: put your code here (lab1).
}

IntAndTable A::IdExp::Interp(Table *t) const {
  return IntAndTable(t->Lookup(this->id), t);
}

int A::IdExp::MaxArgs() const { return 1; }

IntAndTable A::NumExp::Interp(Table *t) const {
  return IntAndTable(this->num, t);
}

int A::NumExp::MaxArgs() const { return 1; }

IntAndTable A::OpExp::Interp(Table *t) const {
  IntAndTable resultLeft = this->left->Interp(t);
  IntAndTable resultRight = this->right->Interp(resultLeft.t);

  switch (this->oper) {
  case 0:
    return IntAndTable(resultLeft.i + resultRight.i, resultRight.t);
  case 1:
    return IntAndTable(resultLeft.i - resultRight.i, resultRight.t);
  case 2:
    return IntAndTable(resultLeft.i * resultRight.i, resultRight.t);
  case 3:
    return IntAndTable(resultLeft.i / resultRight.i, resultRight.t);
  default:
    std::cout << "wrong!" << std::endl;
    exit(0);
  }
}

int A::OpExp::MaxArgs() const { return 1; }

IntAndTable A::EseqExp::Interp(Table *t) const {
  this->stm->Interp(t);
  return this->exp->Interp(t);
}

int A::EseqExp::MaxArgs() const {
  int max1 = this->stm->MaxArgs();
  int max2 = this->exp->MaxArgs();
  return max1 >= max2 ? max1 : max2;
}

IntAndTable A::LastExpList::Interp(Table *t) const {
  IntAndTable result = this->exp->Interp(t);
  std::cout << result.i << " " << std::endl;
  return result;
}

int A::LastExpList::MaxArgs() const { return 1; }

int A::LastExpList::NumExps() const { return 1; }

IntAndTable A::PairExpList::Interp(Table *t) const {
  IntAndTable newIntAndTable = this->exp->Interp(t);
  std::cout << newIntAndTable.i << " ";
  return this->tail->Interp(newIntAndTable.t);
}

int A::PairExpList::MaxArgs() const { return this->tail->MaxArgs() + 1; }

int A::PairExpList::NumExps() const { return 1 + this->tail->NumExps(); }

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
