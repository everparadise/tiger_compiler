#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::BuildLiveGraph() {
  for (auto reg : reg_manager->Registers()->GetList()) {
    if (!temp_node_map_->Look(reg)) {
      auto node = live_graph_.interf_graph->NewNode(reg);
      temp_node_map_->Enter(reg, node);
    }
  }

  for (auto &fnode : flowgraph_->Nodes()->GetList()) {
    auto def = fnode->NodeInfo()->Def();
    auto use = fnode->NodeInfo()->Use();
    for (auto &temp : def->GetList()) {
      if (!temp_node_map_->Look(temp)) {
        auto node = live_graph_.interf_graph->NewNode(temp);
        temp_node_map_->Enter(temp, node);
      }
    }
    for (auto &temp : use->GetList()) {
      if (!temp_node_map_->Look(temp)) {
        auto node = live_graph_.interf_graph->NewNode(temp);
        temp_node_map_->Enter(temp, node);
      }
    }
  }
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  for (auto &fnode : flowgraph_->Nodes()->GetList()) {
    in_->Enter(fnode, new temp::TempList());
    out_->Enter(fnode, new temp::TempList());
  }

  bool changed = true;
  while (changed) {
    changed = false;

    for (auto fnode_it = flowgraph_->Nodes()->GetList().rbegin();
         fnode_it != flowgraph_->Nodes()->GetList().rend(); fnode_it++) {
      temp::TempList *new_out = new temp::TempList();
      for (auto succ : (*fnode_it)->Succ()->GetList()) {
        auto succ_in = in_->Look(succ);
        new_out = new_out->Union(succ_in);
      }

      temp::TempList *new_in = new temp::TempList();
      auto fnode_info = (*fnode_it)->NodeInfo();
      auto oper_instr = dynamic_cast<assem::OperInstr *>(fnode_info);
      auto move_instr = dynamic_cast<assem::MoveInstr *>(fnode_info);
      new_in = new_out->Diff(fnode_info->Def());
      new_in = new_in->Union(fnode_info->Use());

      if (!out_->Look(*fnode_it)->Equal(new_out) ||
          !in_->Look(*fnode_it)->Equal(new_in)) {
        out_->Enter(*fnode_it, new_out);
        in_->Enter(*fnode_it, new_in);
        changed = true;
      } else {
        delete new_in;
        delete new_out;
      }
    }
  }
}
void LiveGraphFactory::InterfGraph() { /* TODO: Put your lab6 code here */
  for (auto &fnode : flowgraph_->Nodes()->GetList()) {

    auto live = out_->Look(fnode);
    if (typeid(*fnode->NodeInfo()) == typeid(assem::MoveInstr)) {
      auto move_instr = static_cast<assem::MoveInstr *>(fnode->NodeInfo());
      auto def_args = move_instr->Def();
      auto use_args = move_instr->Use();
      live = live->Diff(use_args);
      assert(def_args->GetList().size() == 1);
      assert(use_args->GetList().size() == 1);
      auto def_node = temp_node_map_->Look(def_args->GetList().front());
      auto use_node = temp_node_map_->Look(use_args->GetList().front());

      if (live_graph_.moves->Contain(use_node, def_node)) {
        continue;
      }
      live_graph_.moves->Append(use_node, def_node);
    }

    for (auto &temp : fnode->NodeInfo()->Def()->GetList()) {
      for (auto &temp2 : live->GetList()) {
        if (temp == temp2)
          continue;
        auto temp_node = temp_node_map_->Look(temp);
        auto temp2_node = temp_node_map_->Look(temp2);
        if (temp_node->NodeInfo()->Int() < temp2_node->NodeInfo()->Int())
          live_graph_.interf_graph->AddEdge(temp_node, temp2_node);
        else
          live_graph_.interf_graph->AddEdge(temp2_node, temp_node);
      }
    }
  }
}
void LiveGraphFactory::Liveness() {
  LiveMap();
  BuildLiveGraph();
  InterfGraph();
}

} // namespace live