#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() = default;
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
  std::unique_ptr<Result> reg_result;
  std::string function_name;
  std::unique_ptr<cg::AssemInstr> assem_instr;
  live::LiveGraph live_graph;
  tab::Table<temp::Temp, live::INode> *temp_node_map;

  temp::TempList *simplify_worklist;
  temp::TempList *freeze_worklist;
  temp::TempList *spill_worklist;
  temp::TempList *initial_worklist;
  temp::TempList *worklistMoves;
  temp::TempList *spilled_nodes;
  std::stack<temp::Temp *> select_stack;
  std::map<temp::Temp *, int> degree_map;
  int k_reg;

public:
  RegAllocator(std::string function_name,
               std::unique_ptr<cg::AssemInstr> assem_instr)
      : function_name(function_name), assem_instr(std::move(assem_instr)),
        temp_node_map(nullptr), live_graph(nullptr, nullptr) {};
  void RegAlloc();
  void MakeWorklist();
  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
  void AssignColors();
  void DecrementDegree(temp::Temp *temp);
  void EnableMoves(temp::TempList *temp_list);
  void RewriteProgram();
  temp::Temp *GetAlias(temp::Temp *temp);
  std::unique_ptr<Result> TransferResult() { return std::move(reg_result); }
  bool Stop() {
    return simplify_worklist->GetList().empty() &&
           // worklistMoves->GetList().empty() &&
           // freeze_worklist->GetList().empty() &&
           spill_worklist->GetList().empty();
  }
};

} // namespace ra

#endif