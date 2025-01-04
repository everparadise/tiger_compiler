#include "tiger/liveness/flowgraph.h"
#include <iostream>
namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  /* construct flow graph from assem instructions
   */

  fg::FNodePtr prev_node = nullptr;
  std::map<FNodePtr, std::string> jmp_table;

  // iterate through the instruction list
  for (auto &&instr : instr_list_->GetList()) {

    auto node = flowgraph_->NewNode(instr);

    // 1. Add edge between the previous node(if exists) and the current node
    if (prev_node != nullptr) {
      flowgraph_->AddEdge(prev_node, node);
    }

    // 2. set the prev_node to the current node
    prev_node = node;

    // 3. Check if the current instruction is a jump instruction
    //   If it is, add the target label to the jmp_table. We will add the edge
    //   later
    if (typeid(*instr) == typeid(assem::OperInstr)) {
      auto oper_instr = dynamic_cast<assem::OperInstr *>(instr);
      if (oper_instr->assem_[0] == 'j') {
        auto target_pos = oper_instr->assem_.find(" ") + 1;
        std::string target_lebel = oper_instr->assem_.substr(target_pos);
        jmp_table.insert({node, target_lebel});
        if (oper_instr->assem_.find("jmp") != std::string::npos) {
          prev_node = nullptr;
        }
      }
    }

    // 4. Check if the current instruction is a label instruction
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      auto label_instr = dynamic_cast<assem::LabelInstr *>(instr);
      label_map_->insert({label_instr->assem_, node});
    }
  }

  // Add edge for the jump entry
  for (auto &&[node, label] : jmp_table) {
    auto target_node = label_map_->at(label);
    flowgraph_->AddEdge(node, target_node);
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const { return dst_; }

temp::TempList *OperInstr::Def() const { return dst_; }

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  return (src_) ? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  return (src_) ? src_ : new temp::TempList();
}
} // namespace assem
