#include "tiger/frame/x64frame.h"
#include "tiger/env/env.h"

#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

extern frame::RegManager *reg_manager;
extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;

namespace frame {

X64RegManager::X64RegManager() : RegManager() {
  for (int i = 0; i < REG_COUNT; i++)
    regs_.push_back(temp::TempFactory::NewTemp());

  // Note: no frame pointer in tiger compiler
  std::array<std::string_view, REG_COUNT> reg_name{
      "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
      "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
  int reg = RAX;
  for (auto &name : reg_name) {
    temp_map_->Enter(regs_[reg], new std::string(name));
    reg++;
  }
}

temp::TempList *X64RegManager::Registers() {
  const std::array reg_array{
      RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15,
  };
  auto *temp_list = new temp::TempList();
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  const std::array reg_array{RDI, RSI, RDX, RCX, R8, R9};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  std::array reg_array{RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  std::array reg_array{RBP, RBX, R12, R13, R14, R15};
  auto *temp_list = new temp::TempList();
  ;
  for (auto &reg : reg_array)
    temp_list->Append(regs_[reg]);
  return temp_list;
}

temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(regs_[SP]);
  temp_list->Append(regs_[RV]);
  return temp_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() { return regs_[FP]; }

class InFrameAccess : public Access {
public:
  int offset;
  frame::Frame *parent_frame;

  explicit InFrameAccess(int offset, frame::Frame *parent)
      : offset(offset), parent_frame(parent) {}

  llvm::Value *GetAccess() override {
    llvm::Value *fp = parent_frame->sp;
    std::cout << "offset: " << offset << std::endl;
    auto offset_val = ir_builder->getInt64(offset);
    auto int_access = ir_builder->CreateAdd(fp, offset_val);
    return int_access;
    // to be checked
  }

  llvm::Value *GetAccess(llvm::Value *base_addr) override {
    auto offset_val = ir_builder->getInt64(offset);
    auto int_access = ir_builder->CreateAdd(base_addr, offset_val);
    return int_access;
  }

  void setValue(llvm::Value *value, llvm::Type *type) override {
    llvm::Value *fp = parent_frame->sp;
    std::cout << "offset: " << offset << std::endl;
    std::cout << "fp value: " << fp << std::endl;
    auto offset_val = ir_builder->getInt64(offset);
    auto int_access = ir_builder->CreateAdd(fp, offset_val);
    auto val_ptr =
        ir_builder->CreateIntToPtr(int_access, llvm::PointerType::get(type, 0));
    ir_builder->CreateStore(value, val_ptr);
  }
  /* TODO: Put your lab5-part1 code here */
};

class X64Frame : public Frame {
public:
  X64Frame(temp::Label *name, std::list<frame::Access *> *formals)
      : Frame(8, 0, name, formals) {}

  [[nodiscard]] std::string GetLabel() const override { return name_->Name(); }
  [[nodiscard]] temp::Label *Name() const override { return name_; }
  [[nodiscard]] std::list<frame::Access *> *Formals() const override {
    return formals_;
  }
  frame::Access *AllocLocal(bool escape) override {
    frame::Access *access;

    offset_ -= reg_manager->WordSize();
    access = new InFrameAccess(offset_, this);

    return access;
  }

  void AllocOutgoSpace(int size) override {
    if (size > outgo_size_)
      outgo_size_ = size;
  }
};

// shift of view
frame::Frame *NewFrame(temp::Label *name, std::list<bool> formals) {

  std::list<frame::Access *> *args;
  args = new std::list<frame::Access *>();
  X64Frame *new_frame = new X64Frame(name, args);
  int arg_index = 0;
  for (auto &&formal : formals) {
    args->push_back(
        new InFrameAccess(++arg_index * reg_manager->WordSize(), new_frame));
  }
  return new_frame;
  //

  // std::list<frame::Access *> *args;
  // args = new std::list<frame::Access *>();
  // reg_manager->
  // for(auto formal : formals) {

  // }
  // Frame *frame_ = new X64Frame(name, args);
  // frame_->AllocOutgoSpace

  /* TODO: Put your lab5-part1 code here */
}

} // namespace frame