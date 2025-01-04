#include "tiger/codegen/codegen.h"

#include <cassert>
#include <iostream>
#include <sstream>

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;

std::set<std::string> functionSet = {
    "flush",      "exit",         "chr",         "__wrap_getchar", "print",
    "printi",     "ord",          "size",        "concat",         "substring",
    "init_array", "string_equal", "alloc_record"};
namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  temp_map_ = new std::unordered_map<llvm::Value *, temp::Temp *>();
  bb_map_ = new std::unordered_map<llvm::BasicBlock *, int>();
  auto *list = new assem::InstrList();

  // firstly get all global string's location
  for (auto &&frag : frags->GetList()) {
    if (auto *str_frag = dynamic_cast<frame::StringFrag *>(frag)) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "leaq " + std::string(str_frag->str_val_->getName()) + "(%rip),`d0",
          new temp::TempList(tmp), new temp::TempList(), nullptr));
      temp_map_->insert({str_frag->str_val_, tmp});
    }
  }

  // move arguments to temp
  auto arg_iter = traces_->GetBody()->arg_begin();
  auto regs = reg_manager->ArgRegs();
  auto tmp_iter = regs->GetList().begin();

  // first arguement is rsp, we need to skip it
  ++arg_iter;

  for (; arg_iter != traces_->GetBody()->arg_end() &&
         tmp_iter != regs->GetList().end();
       ++arg_iter, ++tmp_iter) {
    auto tmp = temp::TempFactory::NewTemp();
    list->Append(new assem::OperInstr("movq `s0,`d0", new temp::TempList(tmp),
                                      new temp::TempList(*tmp_iter), nullptr));
    temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
  }

  // pass-by-stack parameters
  if (arg_iter != traces_->GetBody()->arg_end()) {
    std::cout << "shouldn't pass-by-stack" << std::endl;
    assert(0);
    auto last_sp = temp::TempFactory::NewTemp();
    list->Append(
        new assem::OperInstr("movq %rsp,`d0", new temp::TempList(last_sp),
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RSP)),
                             nullptr));
    list->Append(new assem::OperInstr(
        "addq $" + std::string(traces_->GetFunctionName()) +
            "_framesize_local,`s0",
        new temp::TempList(last_sp), new temp::TempList(last_sp), nullptr));
    while (arg_iter != traces_->GetBody()->arg_end()) {
      auto tmp = temp::TempFactory::NewTemp();
      list->Append(new assem::OperInstr(
          "movq " +
              std::to_string(8 * (arg_iter - traces_->GetBody()->arg_begin())) +
              "(`s0),`d0",
          new temp::TempList(tmp), new temp::TempList(last_sp), nullptr));
      temp_map_->insert({static_cast<llvm::Value *>(arg_iter), tmp});
      ++arg_iter;
    }
  }

  // construct bb_map
  int bb_index = 0;
  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    bb_map_->insert({bb, bb_index++});
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // record every return value from llvm instruction
    for (auto &&inst : bb->getInstList())
      temp_map_->insert({&inst, temp::TempFactory::NewTemp()});
  }

  for (auto &&bb : traces_->GetBasicBlockList()->GetList()) {
    // Generate label for basic block
    list->Append(new assem::LabelInstr(std::string(bb->getName())));
    // Generate instructions for basic block
    for (auto &&inst : bb->getInstList())
      InstrSel(list, inst, traces_->GetFunctionName(), bb);
  }

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(
      frame::ProcEntryExit1(traces_->GetFunctionName(), list)));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}

void CodeGen::InstrSel(assem::InstrList *instr_list, llvm::Instruction &inst,
                       std::string_view function_name, llvm::BasicBlock *bb) {
  // TODO: your lab5 code here
  if (auto *load_inst = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
    std::cout << "load instruction" << std::endl;
    auto dst = temp_map_->at(&inst);
    if (auto global_var = llvm::dyn_cast<llvm::GlobalVariable>(
            load_inst->getPointerOperand())) {
      instr_list->Append(new assem::OperInstr(
          "leaq " + global_var->getName().str() + "(%rip),`d0",
          new temp::TempList(dst), new temp::TempList(), nullptr));
      instr_list->Append(
          new assem::OperInstr("movq (`s0),`d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else {
      auto src = temp_map_->at(load_inst->getPointerOperand());
      instr_list->Append(
          new assem::OperInstr("movq (`s0),`d0", new temp::TempList(dst),
                               new temp::TempList(src), nullptr));
    }
  } else if (auto *store_inst = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
    std::cout << "store instruction" << std::endl;
    auto dst = temp_map_->at(store_inst->getPointerOperand());
    if (auto *src_const =
            llvm::dyn_cast<llvm::ConstantInt>(store_inst->getValueOperand())) {
      std::cout << "store constant" << std::endl;
      instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(src_const->getZExtValue()) + ",(`d0)",
          new temp::TempList(dst), new temp::TempList(), nullptr));
    } else {
      std::cout << "store value" << std::endl;
      auto src = temp_map_->at(store_inst->getValueOperand());
      instr_list->Append(
          new assem::OperInstr("movq `s0,(`d0)", new temp::TempList(dst),
                               new temp::TempList(src), nullptr));
    }
  } else if (auto *call_inst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
    auto *callee = call_inst->getCalledFunction();
    auto callee_name = callee->getName().str();

    auto regs = reg_manager->ArgRegs();
    auto tmp_iter = regs->GetList().begin();
    auto arg_index = 0;

    temp::TempList *calling_convention = new temp::TempList();
    // first arguement is rsp, we need to skip it
    if (functionSet.find(callee_name) == functionSet.end()) {
      // internal function, skip the first argument
      arg_index++;
    }
    int numOperands = call_inst->getNumOperands();
    for (; arg_index < numOperands - 1; arg_index++) {
      // spill the argument register to stack
      if (tmp_iter == regs->GetList().end()) {
        std::cout << "too many arguments, shouldn't be here" << std::endl;
        assert(0);
        continue;
      }

      calling_convention->Append(*tmp_iter);
      auto arg_value = call_inst->getOperand(arg_index);
      if (auto *arg = llvm::dyn_cast<llvm::ConstantInt>(arg_value)) {
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(arg->getZExtValue()) + ",`d0",
            new temp::TempList(*tmp_iter), new temp::TempList(), nullptr));
      } else {
        // auto arg1 = (*real_arg);
        auto tmp = temp_map_->at(arg_value);
        instr_list->Append(new assem::MoveInstr("movq `s0,`d0",
                                                new temp::TempList(*tmp_iter),
                                                new temp::TempList(tmp)));
      }
      ++tmp_iter;
    }

    // set down stack pointer
    auto tmp0 = temp::TempFactory::NewTemp();
    instr_list->Append(new assem::OperInstr(
        "leaq " + std::string(function_name) + "_framesize_global(%rip),`d0",
        new temp::TempList(tmp0), new temp::TempList(), nullptr));
    instr_list->Append(new assem::OperInstr("movq (`s0),`d0",
                                            new temp::TempList(tmp0),
                                            new temp::TempList(tmp0), nullptr));
    instr_list->Append(
        new assem::OperInstr("subq `s0,`d0",
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RSP)),
                             new temp::TempList(tmp0), nullptr));

    // call function
    temp::TempList *call_dst = reg_manager->CallerSaves();
    if (!callee->getReturnType()->isVoidTy()) {
      call_dst->Append(
          reg_manager->GetRegister(frame::X64RegManager::Reg::RAX));
    }
    instr_list->Append(new assem::OperInstr("call " + std::string(callee_name),
                                            call_dst, calling_convention,
                                            nullptr));

    // set up stack pointer
    auto tmp1 = temp::TempFactory::NewTemp();
    instr_list->Append(new assem::OperInstr(
        "leaq " + std::string(function_name) + "_framesize_global(%rip),`d0",
        new temp::TempList(tmp1), new temp::TempList(), nullptr));
    instr_list->Append(new assem::OperInstr("movq (`s0),`d0",
                                            new temp::TempList(tmp1),
                                            new temp::TempList(tmp1), nullptr));
    instr_list->Append(
        new assem::OperInstr("addq `s0,`d0",
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RSP)),
                             new temp::TempList(tmp1), nullptr));

    // set return value if needed
    if (!callee->getReturnType()->isVoidTy()) {
      auto dst = temp_map_->at(&inst);
      instr_list->Append(
          new assem::OperInstr("movq `s0 ,`d0", new temp::TempList(dst),
                               new temp::TempList(reg_manager->GetRegister(
                                   frame::X64RegManager::Reg::RAX)),
                               nullptr));
    }

  } else if (auto *ret_inst = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
    if (ret_inst->getNumOperands() != 0) {
      if (auto *ret_const =
              llvm::dyn_cast<llvm::ConstantInt>(ret_inst->getOperand(0))) {
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(ret_const->getZExtValue()) + ", `d0",
            new temp::TempList(
                reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
            new temp::TempList(), nullptr));
      } else {
        auto src = temp_map_->at(ret_inst->getOperand(0));
        instr_list->Append(
            new assem::MoveInstr("movq `s0, `d0",
                                 new temp::TempList(reg_manager->GetRegister(
                                     frame::X64RegManager::Reg::RAX)),
                                 new temp::TempList(src)));
      }
    }

    instr_list->Append(new assem::OperInstr(
        "jmp " + std::string(function_name) + "_exit", new temp::TempList(),
        new temp::TempList(), nullptr));

  } else if (auto *br_inst = llvm::dyn_cast<llvm::BranchInst>(&inst)) {
    auto bb_number = bb_map_->at(bb);
    instr_list->Append(
        new assem::OperInstr("movq $" + std::to_string(bb_number) + ",`d0",
                             new temp::TempList(reg_manager->GetRegister(
                                 frame::X64RegManager::Reg::RAX)),
                             new temp::TempList(), nullptr));
    if (br_inst->isConditional()) {
      auto cond = temp_map_->at(br_inst->getCondition());
      auto true_bb = br_inst->getSuccessor(0);
      auto false_bb = br_inst->getSuccessor(1);
      instr_list->Append(
          new assem::OperInstr("cmpq $0,`s0", new temp::TempList(cond),
                               new temp::TempList(cond), nullptr));
      instr_list->Append(new assem::OperInstr("je " + false_bb->getName().str(),
                                              new temp::TempList(),
                                              new temp::TempList(), nullptr));
      instr_list->Append(new assem::OperInstr("jmp " + true_bb->getName().str(),
                                              new temp::TempList(),
                                              new temp::TempList(), nullptr));
    } else {
      auto next_bb = br_inst->getSuccessor(0);
      instr_list->Append(new assem::OperInstr("jmp " + next_bb->getName().str(),
                                              new temp::TempList(),
                                              new temp::TempList(), nullptr));
    }
  } else if (auto *phi_inst = llvm::dyn_cast<llvm::PHINode>(&inst)) {
    // std::cout << "phi instruction" << std::endl;
    PhiInstrct(instr_list, inst, function_name, bb);
    // assert(0);
  } else if (auto *gep_inst = llvm::dyn_cast<llvm::GetElementPtrInst>(&inst)) {
    auto dst = temp_map_->at(&inst);
    auto src = gep_inst->getPointerOperand();
    // llvm::Value *baseIndex = gep_inst->getOperand(1);
    // llvm::Value *offsetIndex = gep_inst->getOperand(2);

    auto element_type =
        gep_inst->getPointerOperandType()->getPointerElementType();
    if (auto *structType = llvm::dyn_cast<llvm::StructType>(element_type)) {
      std::cout << "struct type, get element" << std::endl;
      auto firstIndex = gep_inst->getOperand(1);
      auto secondIndex = gep_inst->getOperand(2);

      llvm::DataLayout dl(gep_inst->getModule());
      int numElements = structType->getNumElements();
      uint64_t offset = numElements * 8;
      std::cout << "sturct type totalSize: " << offset << std::endl;
      instr_list->Append(
          new assem::MoveInstr("movq `s0,`d0", new temp::TempList(dst),
                               new temp::TempList(temp_map_->at(src))));
      if (auto *constIndex = llvm::dyn_cast<llvm::ConstantInt>(firstIndex)) {
        instr_list->Append(new assem::OperInstr(
            "addq $" + std::to_string(constIndex->getSExtValue() * offset) +
                ",`d0",
            new temp::TempList(temp_map_->at(src)),
            new temp::TempList(temp_map_->at(src)), nullptr));
      } else {
        auto firstIndexValue = temp_map_->at(firstIndex);

        instr_list->Append(new assem::OperInstr(
            "leaq (`s0,`s1, " + std::to_string(offset) + "),`d0",
            new temp::TempList(dst),
            new temp::TempList({temp_map_->at(src), firstIndexValue}),
            nullptr));
      }
      if (auto *constIndex = llvm::dyn_cast<llvm::ConstantInt>(secondIndex)) {
        int index = constIndex->getZExtValue();
        int offset = index * 8;
        std::cout << "sturct type index: " << index << ",offset: " << offset
                  << std::endl;
        instr_list->Append(new assem::OperInstr(
            "leaq " + std::to_string(offset) + "(`s0),`d0",
            new temp::TempList(dst), new temp::TempList(dst), nullptr));
      } else {
        std::cout << "get element with variable index" << std::endl;
        assert(false);
      }
    } else {
      auto element_size = 8;
      std::cout << "array type, get index with element_size: " << element_size
                << std::endl;
      auto index = gep_inst->getOperand(1);

      if (auto *constIndex = llvm::dyn_cast<llvm::ConstantInt>(index)) {
        instr_list->Append(new assem::OperInstr(
            "leaq " +
                std::to_string(constIndex->getZExtValue() * element_size) +
                "(`s0),`d0",
            new temp::TempList(dst), new temp::TempList(temp_map_->at(src)),
            nullptr));
      } else {
        auto indexValue = temp_map_->at(index);
        instr_list->Append(new assem::OperInstr(
            "leaq (`s0,`s1," + std::to_string(element_size) + "),`d0",
            new temp::TempList(dst),
            new temp::TempList({temp_map_->at(src), indexValue}), nullptr));
      }
    }
  } else if (auto *op_inst = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
    switch (op_inst->getOpcode()) {
    case llvm::Instruction::Add: {
      std::cout << "add instruction" << std::endl;
      auto add_inst = op_inst;
      auto dst = temp_map_->at(&inst);
      auto lhs = add_inst->getOperand(0);
      auto rhs = add_inst->getOperand(1);
      if (IsRsp(&inst, function_name)) {
        std::cout << "ignore rsp" << std::endl;
        break;
      }
      if (IsRsp(lhs, function_name)) {
        std::cout << "\tadd rsp" << std::endl;
        instr_list->Append(
            new assem::MoveInstr("movq `s0,`d0", new temp::TempList(dst),
                                 new temp::TempList(reg_manager->GetRegister(
                                     frame::X64RegManager::Reg::RSP))));
      } else {
        instr_list->Append(
            new assem::MoveInstr("movq `s0,`d0", new temp::TempList(dst),
                                 new temp::TempList(temp_map_->at(lhs))));
      }

      if (llvm::ConstantInt *rhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(rhs)) {
        std::cout << "\tadd constant" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "addq $" + std::to_string(rhs_const->getSExtValue()) + ",`d0",
            new temp::TempList(dst), new temp::TempList(dst), nullptr));
      } else {
        std::cout << "\tadd value" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "addq `s0,`d0", new temp::TempList(dst),
            new temp::TempList({temp_map_->at(rhs), dst}), nullptr));
      }
      break;
    }
    case llvm::Instruction::Sub: {
      std::cout << "sub instruction" << std::endl;
      auto sub_inst = op_inst;
      auto dst = temp_map_->at(&inst);
      auto lhs = sub_inst->getOperand(0);
      auto rhs = sub_inst->getOperand(1);

      if (IsRsp(lhs, function_name)) {
        std::cout << "\tsub rsp, ignore" << std::endl;
        // instr_list->Append(new assem::OperInstr(
        //     "subq `s0,`d0",
        //     new temp::TempList(
        //         reg_manager->GetRegister(frame::X64RegManager::Reg::RSP)),
        //     new temp::TempList(temp_map_->at(rhs)), nullptr));
        return;
      }
      if (llvm::ConstantInt *lhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(lhs)) {
        std::cout << "\tsub lhs constant" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(lhs_const->getZExtValue()) + ",`d0",
            new temp::TempList(dst), new temp::TempList(), nullptr));
      } else {
        instr_list->Append(
            new assem::MoveInstr("movq `s0,`d0", new temp::TempList(dst),
                                 new temp::TempList(temp_map_->at(lhs))));
      }

      if (llvm::ConstantInt *rhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(rhs)) {
        std::cout << "\tsub rhs constant" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "subq $" + std::to_string(rhs_const->getSExtValue()) + ",`d0",
            new temp::TempList(dst), new temp::TempList(dst), nullptr));
      } else {
        instr_list->Append(new assem::OperInstr(
            "subq `s0,`d0", new temp::TempList(dst),
            new temp::TempList({temp_map_->at(rhs), dst}), nullptr));
      }
      break;
    }
    case llvm::Instruction::Mul: {
      auto mul_inst = op_inst;
      auto dst = temp_map_->at(&inst);

      if (llvm::ConstantInt *lhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(mul_inst->getOperand(0))) {
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(lhs_const->getZExtValue()) + ",`d0",
            new temp::TempList(dst), new temp::TempList(), nullptr));
      } else {
        auto lhs = temp_map_->at(mul_inst->getOperand(0));
        instr_list->Append(new assem::MoveInstr(
            "movq `s0,`d0", new temp::TempList(dst), new temp::TempList(lhs)));
      }
      if (llvm::ConstantInt *rhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(mul_inst->getOperand(1))) {
        instr_list->Append(new assem::OperInstr(
            "imulq $" + std::to_string(rhs_const->getZExtValue()) + ",`d0",
            new temp::TempList(dst), new temp::TempList(), nullptr));
      } else {
        auto rhs = temp_map_->at(mul_inst->getOperand(1));
        instr_list->Append(
            new assem::OperInstr("imulq `s0,`d0", new temp::TempList(dst),
                                 new temp::TempList({rhs, dst}), nullptr));
      }
      break;
    }
    case llvm::Instruction::SDiv: {
      std::cout << "sdiv instruction" << std::endl;
      auto sdiv_inst = op_inst;
      auto dst = temp_map_->at(&inst);
      if (llvm::ConstantInt *lhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(sdiv_inst->getOperand(0))) {
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(lhs_const->getZExtValue()) + ",`d0",
            new temp::TempList(
                reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
            new temp::TempList(), nullptr));
      } else {
        instr_list->Append(new assem::MoveInstr(
            "movq `s0,`d0",
            new temp::TempList(
                reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
            new temp::TempList(temp_map_->at(sdiv_inst->getOperand(0)))));
      }
      instr_list->Append(new assem::OperInstr(
          "cqto",
          new temp::TempList(
              {reg_manager->GetRegister(frame::X64RegManager::Reg::RAX),
               reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
          new temp::TempList(
              {reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)}),
          nullptr));
      if (llvm::ConstantInt *rhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(sdiv_inst->getOperand(1))) {
        auto tmpTemp = temp::TempFactory::NewTemp();
        instr_list->Append(new assem::OperInstr(
            "movq $" + std::to_string(rhs_const->getZExtValue()) + ",`d0",
            new temp::TempList(tmpTemp), new temp::TempList(tmpTemp), nullptr));
        instr_list->Append(new assem::OperInstr(
            "idivq `s0",
            new temp::TempList(
                {reg_manager->GetRegister(frame::X64RegManager::Reg::RAX),
                 reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
            new temp::TempList(
                {tmpTemp,
                 reg_manager->GetRegister(frame::X64RegManager::Reg::RAX),
                 reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
            nullptr));
      } else {
        instr_list->Append(new assem::OperInstr(
            "idivq `s0",
            new temp::TempList(
                {reg_manager->GetRegister(frame::X64RegManager::Reg::RAX),
                 reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
            new temp::TempList(
                {temp_map_->at(sdiv_inst->getOperand(1)),
                 reg_manager->GetRegister(frame::X64RegManager::Reg::RAX),
                 reg_manager->GetRegister(frame::X64RegManager::Reg::RDX)}),
            nullptr));
      }
      instr_list->Append(
          new assem::MoveInstr("movq %rax,`d0", new temp::TempList(dst),
                               new temp::TempList(reg_manager->GetRegister(
                                   frame::X64RegManager::Reg::RAX))));
      break;
    }
    }
  } else if (auto *icmp_inst = llvm::dyn_cast<llvm::ICmpInst>(&inst)) {
    std::cout << "icmp instruction" << std::endl;
    auto dst = temp_map_->at(&inst);
    // auto lhs = temp_map_->at(icmp_inst->getOperand(0));
    // auto rhs = temp_map_->at(icmp_inst->getOperand(1));
    if (llvm::ConstantInt *lhs_const =
            llvm::dyn_cast<llvm::ConstantInt>(icmp_inst->getOperand(0))) {
      if (llvm::ConstantInt *rhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(icmp_inst->getOperand(1))) {
        std::cout << "icmp instruction with two constant" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "cmpq $" + std::to_string(rhs_const->getZExtValue()) + ",$" +
                std::to_string(lhs_const->getZExtValue()),
            new temp::TempList(), new temp::TempList(), nullptr));
      } else {
        std::cout << "icmp instruction with lhs constant" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "cmpq s0, $" + std::to_string(lhs_const->getZExtValue()),
            new temp::TempList(),
            new temp::TempList(temp_map_->at(icmp_inst->getOperand(1))),
            nullptr));
      }
    } else {
      if (llvm::ConstantInt *rhs_const =
              llvm::dyn_cast<llvm::ConstantInt>(icmp_inst->getOperand(1))) {
        std::cout << "icmp instruction with rhs constant" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "cmpq $" + std::to_string(rhs_const->getZExtValue()) + ", `s0",
            new temp::TempList(),
            new temp::TempList(temp_map_->at(icmp_inst->getOperand(0))),
            nullptr));
      } else {
        std::cout << "icmp instruction with two value" << std::endl;
        instr_list->Append(new assem::OperInstr(
            "cmpq `s1,`s0", new temp::TempList(),
            new temp::TempList({temp_map_->at(icmp_inst->getOperand(0)),
                                temp_map_->at(icmp_inst->getOperand(1))}),
            nullptr));
      }
    }

    instr_list->Append(new assem::OperInstr(
        "movq $0,`d0", new temp::TempList(dst), new temp::TempList(), nullptr));
    if (icmp_inst->getPredicate() == llvm::ICmpInst::ICMP_EQ) {
      instr_list->Append(
          new assem::OperInstr("sete `d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else if (icmp_inst->getPredicate() == llvm::ICmpInst::ICMP_NE) {
      instr_list->Append(
          new assem::OperInstr("setne `d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else if (icmp_inst->getPredicate() == llvm::ICmpInst::ICMP_SGT) {
      instr_list->Append(
          new assem::OperInstr("setg `d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else if (icmp_inst->getPredicate() == llvm::ICmpInst::ICMP_SGE) {
      instr_list->Append(
          new assem::OperInstr("setge `d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else if (icmp_inst->getPredicate() == llvm::ICmpInst::ICMP_SLT) {
      instr_list->Append(
          new assem::OperInstr("setl `d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else if (icmp_inst->getPredicate() == llvm::ICmpInst::ICMP_SLE) {
      instr_list->Append(
          new assem::OperInstr("setle `d0", new temp::TempList(dst),
                               new temp::TempList(dst), nullptr));
    } else {
      throw std::runtime_error(std::string("Unknown icmp predicate: ") +
                               std::to_string(icmp_inst->getPredicate()));
    }

  } else if (auto *zext_inst = llvm::dyn_cast<llvm::ZExtInst>(&inst)) {
    auto dst = temp_map_->at(&inst);
    auto src = temp_map_->at(zext_inst->getOperand(0));
    instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0", new temp::TempList(dst), new temp::TempList(src)));
  } else if (auto *ptr_to_int_inst =
                 llvm::dyn_cast<llvm::PtrToIntInst>(&inst)) {
    auto dst = temp_map_->at(&inst);
    auto src = temp_map_->at(ptr_to_int_inst->getOperand(0));
    instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0", new temp::TempList(dst), new temp::TempList(src)));
  } else if (auto *int_to_ptr_inst =
                 llvm::dyn_cast<llvm::IntToPtrInst>(&inst)) {
    auto dst = temp_map_->at(&inst);
    auto src = temp_map_->at(int_to_ptr_inst->getOperand(0));
    instr_list->Append(new assem::MoveInstr(
        "movq `s0,`d0", new temp::TempList(dst), new temp::TempList(src)));
  } else {
    throw std::runtime_error(std::string("Unknown instruction: ") +
                             inst.getOpcodeName());
  }
}

void CodeGen::PhiInstrct(assem::InstrList *instr_list, llvm::Instruction &inst,
                         std::string_view function_name, llvm::BasicBlock *bb) {
  // assert(0);

  auto phi_inst = llvm::cast<llvm::PHINode>(&inst);
  auto incomming_number = phi_inst->getNumIncomingValues();
  std::cout << "phi instruction with " << incomming_number << std::endl;
  for (auto i = 0; i < incomming_number; i++) {
    auto incomming_bb = phi_inst->getIncomingBlock(i);
    auto incomming_bb_number = bb_map_->at(incomming_bb);
    instr_list->Append(new assem::OperInstr(
        "cmp $" + std::to_string(incomming_bb_number) + ",`s0",
        new temp::TempList(),
        new temp::TempList(
            reg_manager->GetRegister(frame::X64RegManager::Reg::RAX)),
        nullptr));
    instr_list->Append(new assem::OperInstr(
        "je " + bb->getName().str() + "_phi_case_" +
            std::to_string(incomming_bb_number),
        new temp::TempList(), new temp::TempList(), nullptr));
  }
  for (auto i = 0; i < incomming_number; i++) {
    auto incomming_bb_number = bb_map_->at(phi_inst->getIncomingBlock(i));
    instr_list->Append(
        new assem::LabelInstr(bb->getName().str() + "_phi_case_" +
                              std::to_string(incomming_bb_number)));

    auto incomming_value = phi_inst->getIncomingValue(i);
    if (auto const_incomming =
            llvm::dyn_cast<llvm::ConstantInt>(incomming_value)) {
      instr_list->Append(new assem::OperInstr(
          "movq $" + std::to_string(const_incomming->getZExtValue()) + ",`d0",
          new temp::TempList(temp_map_->at(&inst)), new temp::TempList(),
          nullptr));
    } else if (auto null_pointer =
                   llvm::dyn_cast<llvm::ConstantPointerNull>(incomming_value)) {
      // we need pay additional attention to null pointer
      // just move a zero to the destination
      instr_list->Append(new assem::OperInstr(
          "movq $0,`d0", new temp::TempList(temp_map_->at(&inst)),
          new temp::TempList(), nullptr));
    } else {
      instr_list->Append(new assem::MoveInstr(
          "movq `s0,`d0", new temp::TempList(temp_map_->at(&inst)),
          new temp::TempList(temp_map_->at(incomming_value))));
    }
    instr_list->Append(new assem::OperInstr(
        "jmp " + std::string(bb->getName().str() + "_phiend"),
        new temp::TempList(), new temp::TempList(), nullptr));
  }

  instr_list->Append(new assem::LabelInstr(bb->getName().str() + "_phiend"));
}
} // namespace cg