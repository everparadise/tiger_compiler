#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"

namespace col {
struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map *coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map *coloring;
  live::INodeListPtr spills;
};

class Color {
  /* TODO: Put your lab6 code here */
  //   Result color_result;
  //   frame::RegManager *reg_manager;
  //   live::LiveGraph live_graph;

  //   void Simplify();

  //   /* only use George method to coalesce*/
  //   void Coalesce();
  //   void Freeze();
  //   void PotentialSpill();
  //   void ActualSpill();
  //   void Select();

  // public:
  //   Color(live::LiveGraph live_graph, frame::RegManager *reg_manager,
  //         tab::Table<temp::Temp, live::INode> *temp_node_map)
  //       : live_graph(live_graph), reg_manager(reg_manager) {}

  //   void ColorGraph();
  //   void MakeWorklist();
  //   Result Transfer_result() { return color_result; }
};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
