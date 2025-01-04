#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <iostream>

extern frame::RegManager *reg_manager;
extern std::map<std::string, std::pair<int, int>> frame_info_map;

namespace ra {
/* TODO: Put your lab6 code here */
bool precolored(temp::Temp *temp) {
  // rsp needs to be extra handled
  // rsp's INT is 107
  return reg_manager->Registers()->Contain(temp) || temp->Int() == 107;
}

void RegAllocator::RegAlloc() {
  // 1. Build interference graph
  this->reg_result = std::make_unique<Result>(temp::Map::Empty(), nullptr);
  fg::FlowGraphFactory flow_graph_factory(assem_instr->GetInstrList());
  flow_graph_factory.AssemFlowGraph();

  live::LiveGraphFactory live_graph_factory(flow_graph_factory.GetFlowGraph());
  live_graph_factory.Liveness();
  live::LiveGraph live_graph = live_graph_factory.GetLiveGraph();
  tab::Table<temp::Temp, live::INode> *temp_node_map =
      live_graph_factory.GetTempNodeMap();

  // 2. Simplify
  // 3. Coalesce
  // 4. Freeze
  // 5. Spill
  // 6. Assign colors

  this->live_graph.interf_graph = live_graph.interf_graph;
  this->live_graph.moves = live_graph.moves;
  this->temp_node_map = temp_node_map;
  MakeWorklist();

  while (!Stop()) {
    if (simplify_worklist->GetList().size()) {
      Simplify();
    }
    // else if (worklistMoves->GetList().size()) {
    //   Coalesce();
    // }
    // else if (freeze_worklist->GetList().size()) {
    //   Freeze();
    // }
    else if (spill_worklist->GetList().size()) {
      SelectSpill();
    }
  }
  AssignColors();

  // 7. Rewrite program
  // 8. Return result
  if (spilled_nodes->GetList().size()) {
    // exit(0);
    RewriteProgram();
    RegAlloc();
  } else {
    reg_result->il_ = assem_instr->GetInstrList();
  }
}

void RegAllocator::MakeWorklist() {
  // 创建initialList，包含所有的临时寄存器
  k_reg = reg_manager->Registers()->GetList().size();
  initial_worklist = new temp::TempList();
  simplify_worklist = new temp::TempList();
  freeze_worklist = new temp::TempList();
  spill_worklist = new temp::TempList();
  spilled_nodes = new temp::TempList();
  select_stack = std::stack<temp::Temp *>();
  worklistMoves = new temp::TempList();
  degree_map = std::map<temp::Temp *, int>();
  for (auto &node : live_graph.interf_graph->Nodes()->GetList()) {
    this->degree_map[node->NodeInfo()] = node->Degree();
    if (precolored(node->NodeInfo())) {
      continue;
    }
    initial_worklist->Append(node->NodeInfo());
  }

  for (auto it = initial_worklist->GetList().begin();
       it != initial_worklist->GetList().end(); it++) {
    auto temp_node = temp_node_map->Look(*it);
    if (temp_node->Degree() >= k_reg) {
      spill_worklist->Append(*it);
    }
    // else if (MoveRelated(live_graph.moves, *it)) {
    //   freeze_worklist->Append(*it);
    // }
    else {
      simplify_worklist->Append(*it);
    }
  }
}

void RegAllocator::Simplify() {
  auto &node = simplify_worklist->GetList().front();
  simplify_worklist->Delete(node);
  select_stack.push(node);
  for (auto &adj : temp_node_map->Look(node)->Adj()->GetList()) {
    DecrementDegree(adj->NodeInfo());
  }
}

void RegAllocator::DecrementDegree(temp::Temp *temp) {
  if (this->degree_map.find(temp) == this->degree_map.end()) {
    assert(precolored(temp));
    return;
  }
  this->degree_map[temp]--;
  if (this->degree_map[temp] == k_reg - 1) {
    spill_worklist->Delete(temp);
    // if (MoveRelated(live_graph.moves, temp)) {
    //   freeze_worklist->Append(temp);
    // } else {
    simplify_worklist->Append(temp);
    //}
  }
}

/*
 * Select a node with highest degree in spill worklist to do potential spill
 */
void RegAllocator::SelectSpill() {
  int max_degree = 0;
  auto node_to_spill = spill_worklist->GetList().begin();
  for (auto it = spill_worklist->GetList().begin();
       it != spill_worklist->GetList().end(); it++) {
    if (degree_map[*it] > max_degree) {
      max_degree = degree_map[*it];
      node_to_spill = it;
    }
  }
  simplify_worklist->Append(*node_to_spill);
  spill_worklist->Delete(*node_to_spill);
  // FreezeMoves(spill_node);
}

void RegAllocator::AssignColors() {
  std::map<temp::Temp *, temp::Temp *> colord_registers;
  while (!select_stack.empty()) {
    auto temp = select_stack.top();
    select_stack.pop();
    auto ok_colors = reg_manager->Registers();
    for (auto &adj : temp_node_map->Look(temp)->Adj()->GetList()) {
      if (precolored(adj->NodeInfo())) {
        ok_colors->Delete(adj->NodeInfo());
      } else if (reg_result->coloring_->Look(adj->NodeInfo())) {
        ok_colors->Delete(colord_registers[adj->NodeInfo()]);
      }
    }
    if (ok_colors->GetList().empty()) {
      spilled_nodes->Append(temp);
    } else {
      std::cout << "temp: " << temp->Int() << "\tcolor: "
                << *reg_manager->temp_map_->Look(ok_colors->GetList().front())
                << std::endl;
      reg_result->coloring_->Enter(
          temp, reg_manager->temp_map_->Look(ok_colors->GetList().front()));
      colord_registers[temp] = ok_colors->GetList().front();
    }
  }
}

/*
 * Rewrite program to actual spill
 * 1. allocate space for spilled nodes
 * 2. For instr that use spilled node, load from memory
 * 3. For instr that def spilled node, store to memory
 */
void RegAllocator::RewriteProgram() {
  for (auto &spilled_node : spilled_nodes->GetList()) {

    auto temp = temp_node_map->Look(spilled_node)->NodeInfo();
    auto offset = frame_info_map[function_name].first - 8;
    frame_info_map[function_name].first -= 8;
    frame_info_map[function_name].second += 8;
    for (auto iter = assem_instr->GetInstrList()->GetList().begin();
         iter != assem_instr->GetInstrList()->GetList().end();) {
      auto instr = *iter;
      if (instr->Use()->Contain(temp)) {
        auto new_temp = temp::TempFactory::NewTemp();
        auto new_instr = new assem::OperInstr(
            "movq " + std::to_string(offset) + "(`s0), `d0",
            new temp::TempList(new_temp),
            new temp::TempList(
                reg_manager->GetRegister(frame::X64RegManager::Reg::RSP)),
            nullptr);
        assem_instr->GetInstrList()->Insert(iter, new_instr);
        if (typeid(*instr) == typeid(assem::MoveInstr)) {
          auto move_instr = static_cast<assem::MoveInstr *>(instr);
          move_instr->src_->Replace(temp, new_temp);
        } else if (typeid(*instr) == typeid(assem::OperInstr)) {
          auto oper_instr = static_cast<assem::OperInstr *>(instr);
          oper_instr->src_->Replace(temp, new_temp);
        } else {
          assert(false);
        }
      }
      if (instr->Def()->Contain(temp)) {
        auto new_temp = temp::TempFactory::NewTemp();
        auto new_instr = new assem::OperInstr(
            "movq `s0, " + std::to_string(offset) + "(`d0)",
            new temp::TempList(
                reg_manager->GetRegister(frame::X64RegManager::Reg::RSP)),
            new temp::TempList(new_temp), nullptr);

        if (typeid(*instr) == typeid(assem::MoveInstr)) {
          auto move_instr = static_cast<assem::MoveInstr *>(instr);
          move_instr->dst_->Replace(temp, new_temp);
        } else if (typeid(*instr) == typeid(assem::OperInstr)) {
          auto oper_instr = static_cast<assem::OperInstr *>(instr);
          oper_instr->dst_->Replace(temp, new_temp);
        } else {
          assert(false);
        }
        iter++;
        assem_instr->GetInstrList()->Insert(iter, new_instr);
      } else
        iter++;
    }
  }
}

temp::Temp *RegAllocator::GetAlias(temp::Temp *temp) {
  exit(0);
  return nullptr;
  // UNIMPLEMENTED
  //   if (temp->Int() == 0) {
  //     return temp;
  //   }
  //   if (temp->Int() == 1) {
  //     return temp;
  //   }
  //   return temp;
}
void RegAllocator::EnableMoves(temp::TempList *nodes) {
  exit(0);
  // UNIMPLEMENTED
  //   for (auto &node : nodes->GetList()) {
  //     for (auto &move : node->MoveList()->GetList()) {
  //       if (move->IsMoveRelated()) {
  //         move->Enable();
  //       }
  //     }
  //   }
}

void RegAllocator::Freeze() { exit(0); }

void RegAllocator::Coalesce() { exit(0); }

bool MoveRelated(live::MoveList *move_list, temp::Temp *temp) {
  for (auto &move : move_list->GetList()) {
    if (move.first->NodeInfo() == temp || move.second->NodeInfo() == temp) {
      return true;
    }
  }
  return false;
}
} // namespace ra