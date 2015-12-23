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
#include "Utils.h"

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

#define OPTION(variable, type, option, description) \
    static cl::opt<type> variable (option, cl::desc(description), cl::cat(ToolCategory))

OPTION( Number      , bool        , "number"       , "number all statements");
OPTION( Ids         , bool        , "ids"          , "print count of statement ids");
OPTION( Annotate    , bool        , "annotate"     , "annotate each statement with its class");
OPTION( List        , bool        , "list"         , "list every statement's id, class, and range");
OPTION( Json        , bool        , "json"         , "list JSON-encoded descriptions for every statement.");
OPTION( Cut         , bool        , "cut"          , "cut stmt1");
OPTION( Insert      , bool        , "insert"       , "copy stmt1 to before stmt2");
OPTION( InsertV     , bool        , "insert-value" , "insert value1 before stmt1");
OPTION( Swap        , bool        , "swap"         , "swap stmt1 and stmt2");
OPTION( Set         , bool        , "set"          , "set the text of stmt1 to value1");
OPTION( Set2        , bool        , "set2"         , "set the text of stmt1 to value1 and stmt2 to value2");
OPTION( SetRange    , bool        , "set-range"    , "set the range from the start of stmt1 to the end of stmt2 to value1");
OPTION( Stmt1       , unsigned int, "stmt1"        , "statement 1 for mutation ops");
OPTION( Stmt2       , unsigned int, "stmt2"        , "statement 2 for mutation ops");
OPTION( Value1      , std::string , "value1"       , "string value for mutation ops");
OPTION( Value2      , std::string , "value2"       , "second string value for mutation ops");
OPTION( File1       , std::string , "file1"        , "file containing value1");
OPTION( File2       , std::string , "file2"        , "file containing value2");
OPTION( Binary      , std::string , "binary"       , "binary with DWARF information for line->address mapping");
OPTION( Fields      , std::string , "fields"       , "comma-delimited list of JSON fields to output");

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
        if (!File1.empty())
            Value1 = Utils::filenameToContents(File1);
        if (!File2.empty())
            Value2 = Utils::filenameToContents(File2);

        if (Number)
            return clang_mutate::CreateASTNumberer();
        if (Ids)
            return clang_mutate::CreateASTIDS();
        if (Annotate)
            return clang_mutate::CreateASTAnnotator();
        if (List)
            return clang_mutate::CreateASTLister(Stmt1, 
                                                 Fields,
                                                 Binary, 
                                                 false, 
                                                 CI);
        if (Json)
            return clang_mutate::CreateASTLister(Stmt1, 
                                                 Fields,
                                                 Binary, 
                                                 true, 
                                                 CI);
        if (Cut)
            return clang_mutate::CreateASTCutter(Stmt1);
        if (SetRange)
            return clang_mutate::CreateASTRangeSetter(Stmt1, Stmt2, Value1);
        if (Insert)
            return clang_mutate::CreateASTInserter(Stmt1, Stmt2);
        if (Swap)
            return clang_mutate::CreateASTSwapper(Stmt1, Stmt2);
        if (Set)
            return clang_mutate::CreateASTSetter(Stmt1, Value1);
        if (Set2)
            return clang_mutate::CreateASTSetter2(Stmt1, Value1, Stmt2, Value2);
        if (InsertV)
            return clang_mutate::CreateASTValueInserter(Stmt1, Value1);

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
        errs() << "\tset2\n";
        errs() << "\tset-range\n";
        errs() << "\tinsert-value\n";

        exit(EXIT_FAILURE);
  }

  clang::CompilerInstance * CI;
};
}

int main(int argc, const char **argv) {
    CommonOptionsParser OptionsParser(argc, argv, ToolCategory);
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());
    ActionFactory Factory;
    return Tool.run(newFrontendActionFactory<ActionFactory>(&Factory, &Factory).get());
}
