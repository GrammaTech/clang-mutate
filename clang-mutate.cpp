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

static cl::OptionCategory Mutator("AST mutation");
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

static cl::opt<bool>        Number ("number",       cl::desc("number all statements"));
static cl::opt<bool>           Ids ("ids",          cl::desc("print count of statement ids"));
static cl::opt<bool>      Annotate ("annotate",     cl::desc("annotate each statement with its class"));
static cl::opt<bool>          List ("list",         cl::desc("list every statement's id, class and range"));
static cl::opt<bool>           Cut ("cut",          cl::desc("cut stmt1"));
static cl::opt<bool>        Insert ("insert",       cl::desc("copy stmt1 to after stmt2"));
static cl::opt<bool>          Swap ("swap",         cl::desc("Swap stmt1 and stmt2"));
static cl::opt<bool>           Get ("get",          cl::desc("Return the text of stmt1"));
static cl::opt<bool>           Set ("set",          cl::desc("Set the text of stmt1 to value"));
static cl::opt<bool>   InsertValue ("insert-value", cl::desc("insert value before stmt1"));
static cl::opt<unsigned int> Stmt1 ("stmt1",        cl::desc("statement 1 for mutation ops"));
static cl::opt<unsigned int> Stmt2 ("stmt2",        cl::desc("statement 2 for mutation ops"));
static cl::opt<std::string>  Value ("value",        cl::desc("string value for mutation ops"));
static cl::opt<std::string>  Stmts ("stmts",        cl::desc("string of space-separated statement ids"));

namespace {
class ActionFactory {
public:
  clang::ASTConsumer *newASTConsumer() {
    if (Number)
      return clang::CreateASTNumberer();
    if (Ids)
      return clang::CreateASTIDS();
    if (Annotate)
      return clang::CreateASTAnnotator();
    if (List)
      return clang::CreateASTLister();
    if (Cut)
      return clang::CreateASTCuter(Stmt1);
    if (Insert)
      return clang::CreateASTInserter(Stmt1, Stmt2);
    if (Swap)
      return clang::CreateASTSwapper(Stmt1, Stmt2);
    if (Get)
      return clang::CreateASTGetter(Stmt1);
    if (Set)
      return clang::CreateASTSetter(Stmt1, Value);
    if (InsertValue)
      return clang::CreateASTValueInserter(Stmt1, Value);
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
    errs() << "\tinsert-value\n";
    exit(EXIT_FAILURE);
  }
};
}

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, Mutator);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  outs() << Stmts << "\n";
  ActionFactory Factory;
  return Tool.run(newFrontendActionFactory<ActionFactory>().get());
}
