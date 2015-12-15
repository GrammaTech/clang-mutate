//===--------------- clang-mutate.cpp - Clang mutation tool ---------------===//
//
// Copyright (C) 2012 Eric Schulte
//
//===----------------------------------------------------------------------===//
//
//  This file implements a mutation tool that runs a number of
//  mutation actions defined in ASTMutate.cpp over C language source
//  code files.
//
//  This tool uses the Clang Tooling infrastructure, see
//  http://clang.llvm.org/docs/LibTooling.html for details.
//
//===----------------------------------------------------------------------===//
#include "ASTMutate.h"
#include "ASTLister.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ToolCategory("clang-mutate options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp(
    "Example Usage:\n"
    "\n"
    "\tto number all statements in a file, use:\n"
    "\n"
    "\t  ./clang-mutate -number file.c\n"
    "\n"
    "\tto count statement IDs, use:\n"
    "\n"
    "\t  ./clang-mutate -ids file.c\n"
    "\n"
    "\tto cut the statement with ID=12, use:\n"
    "\n"
    "\t  ./clang-mutate -cut -stmt1=12 file.c\n"
    "\n"
);

static cl::opt<bool>           Number ("number",       cl::desc("number all statements"));
static cl::opt<bool>              Ids ("ids",          cl::desc("print count of statement ids"));
static cl::opt<bool>         Annotate ("annotate",     cl::desc("annotate each statement with its class"));
static cl::opt<bool>             List ("list",         cl::desc("list every statement's id, class and range"));
static cl::opt<bool>              Cut ("cut",           cl::desc("cut stmt1"));
static cl::opt<bool>     CutEnclosing ("cut-enclosing", cl::desc("cut complete statement containing stmt1"));
static cl::opt<bool>           Insert ("insert",       cl::desc("copy stmt1 to after stmt2"));
static cl::opt<bool>             Swap ("swap",         cl::desc("Swap stmt1 and stmt2"));
static cl::opt<bool>              Get ("get",          cl::desc("Return the text of stmt1"));
static cl::opt<bool>              Set ("set",          cl::desc("Set the text of stmt1 to value"));
static cl::opt<bool>         SetRange ("set-range",     cl::desc("set the range from the start of stmt1 to the end of stmt2 to value"));
static cl::opt<unsigned int> GetScope ("get-scope",    cl::desc("Get the first n variables in scope at stmt1"));
static cl::opt<bool>          GetInfo  ("get-info",      cl::desc("Get information about stmt1"));
static cl::opt<bool>      InsertValue  ("insert-value",  cl::desc("insert value before stmt1"));
static cl::opt<bool>      InsertBefore ("insert-before", cl::desc("insert value before the complete statement enclosing stmt1"));
static cl::opt<unsigned int>    Stmt1 ("stmt1",        cl::desc("statement 1 for mutation ops"));
static cl::opt<unsigned int>    Stmt2 ("stmt2",        cl::desc("statement 2 for mutation ops"));
static cl::opt<std::string>     Value ("value",        cl::desc("string value for mutation ops"));
static cl::opt<std::string>     Stmts ("stmts",        cl::desc("string of space-separated statement ids"));
static cl::opt<std::string>    Binary ("binary",       cl::desc("binary with DWARF information for line->address mapping"));
static cl::opt<bool>          JSONOut ("json",         cl::desc("output results in JSON (-list only)"));

namespace {
class ActionFactory : public SourceFileCallbacks {
public:

    virtual bool handleBeginSource(clang::CompilerInstance & CIref,
                                   StringRef Filename)
    {
        CI = &CIref;
        return SourceFileCallbacks::handleBeginSource(CIref, Filename);
    }

    std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    if (Number)
      return clang_mutate::CreateASTNumberer();
    if (Ids)
      return clang_mutate::CreateASTIDS();
    if (Annotate)
      return clang_mutate::CreateASTAnnotator();
    if (List)
      return clang_mutate::CreateASTLister(Binary, JSONOut, CI);
    if (Cut)
      return clang_mutate::CreateASTCutter(Stmt1);
    if (SetRange)
      return clang_mutate::CreateASTRangeSetter(Stmt1, Stmt2, Value);
    if (CutEnclosing)
      return clang_mutate::CreateASTEnclosingCutter(Stmt1);
    if (Insert)
      return clang_mutate::CreateASTInserter(Stmt1, Stmt2);
    if (Swap)
      return clang_mutate::CreateASTSwapper(Stmt1, Stmt2);
    if (Get)
      return clang_mutate::CreateASTGetter(Stmt1);
    if (Set)
      return clang_mutate::CreateASTSetter(Stmt1, Value);
    if (InsertValue)
      return clang_mutate::CreateASTValueInserter(Stmt1, Value);
    if (InsertBefore)
      return clang_mutate::CreateASTValuePreInserter(Stmt1, Value);
    if (GetInfo)
      return clang_mutate::CreateASTInfoGetter(Stmt1);
    if (GetScope)
      return clang_mutate::CreateASTScopeGetter(Stmt1, GetScope);
    
    errs() << "Must supply one of;";
    errs() << "\tnumber\n";
    errs() << "\tids\n";
    errs() << "\tannotate\n";
    errs() << "\tlist\n";
    errs() << "\tcut\n";
    errs() << "\tinsert\n";
    errs() << "\tswap\n";
    errs() << "\tget\n";
    errs() << "\tset\n";
    errs() << "\tset-range\n";
    errs() << "\tinsert-value\n";
    errs() << "\tinsert-before\n";
    errs() << "\tcut-enclosing\n";
    errs() << "\tget-info\n";
    errs() << "\tget-scope\n";
    exit(EXIT_FAILURE);
  }

  clang::CompilerInstance * CI;
};
}

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, ToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  outs() << Stmts << "\n";
  ActionFactory Factory;
  return Tool.run(newFrontendActionFactory<ActionFactory>(&Factory, &Factory).get());
}
