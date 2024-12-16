
#include "tiger/absyn/absyn.h"
#include "tiger/escape/escape.h"
#include "tiger/frame/x64frame.h"
#include "tiger/parse/parser.h"
#include "tiger/semant/semant.h"
#include "tiger/translate/translate.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

frame::RegManager *reg_manager;
frame::Frags *frags;
llvm::IRBuilder<> *ir_builder;
llvm::Module *ir_module;
extern std::vector<std::pair<std::string, frame::Frame *>> frame_info;

int main(int argc, char **argv) {
  std::string_view fname;
  std::unique_ptr<absyn::AbsynTree> absyn_tree;
  reg_manager = new frame::X64RegManager();
  frags = new frame::Frags();

  llvm::LLVMContext context;
  ir_module = new llvm::Module("lightir", context);
  ir_module->setTargetTriple("x86_64-pc-linux-gnu");
  ir_builder = new llvm::IRBuilder<>(context);

  if (argc < 2) {
    fprintf(stderr, "usage: tiger-compiler file.tig\n");
    exit(1);
  }

  fname = std::string_view(argv[1]);
  std::cout << fname << std::endl;
  {
    std::unique_ptr<err::ErrorMsg> errormsg;

    {
      // Lab 3: parsing
      // TigerLog("-------====Parse=====-----\n");
      Parser parser(fname, std::cerr);
      parser.parse();
      absyn_tree = parser.TransferAbsynTree();
      errormsg = parser.TransferErrormsg();
    }

    {
      // Lab 4: semantic analysis
      // TigerLog("-------====Semantic Analysis=====-----\n");
      sem::ProgSem prog_sem(std::move(absyn_tree), std::move(errormsg));
      prog_sem.SemAnalyze();
      absyn_tree = prog_sem.TransferAbsynTree();
      errormsg = prog_sem.TransferErrormsg();
    }

    {
      // Lab 5: escape analysis
      // TigerLog("-------====Escape analysis=====-----\n");
      esc::EscFinder esc_finder(std::move(absyn_tree));
      esc_finder.FindEscape();
      absyn_tree = esc_finder.TransferAbsynTree();
    }

    {
      // Lab 5: translate IR tree
      // TigerLog("-------====Translate=====-----\n");
      tr::ProgTr prog_tr(std::move(absyn_tree), std::move(errormsg));
      prog_tr.Translate();
      errormsg = prog_tr.TransferErrormsg();

      std::string outfile = static_cast<std::string>(fname) + ".ll";
      std::error_code EC;
      llvm::raw_fd_ostream file(outfile, EC);
      ir_module->print(file, nullptr);

      file << "\n; ===== Frame Information =====\n";
      for (auto frame_pair : frame_info) {
        file << "; " << frame_pair.first << " " << frame_pair.second->offset_
             << " " << frame_pair.second->calculateActualFramesize() << "\n";
      }
      file.close();
    }

    if (errormsg->AnyErrors())
      return 1; // Don't continue if error occurrs
  }

  {}

  // {
  //   // Output assembly
  //   output::AssemGen assem_gen(fname);
  //   assem_gen.GenAssem(true);
  // }

  return 0;
}