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
#include "Interactive.h"
#include "FAF.h"
#include "Utils.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <sstream>

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
OPTION( NumberF     , bool        , "number-full"  , "number all full statements");
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
OPTION( SetFunc     , bool        , "set-func"     , "set the text of the function containing stmt1 to value1");
OPTION( Interactive , bool        , "interactive"  , "run in interactive mode");
OPTION( Stmt1       , unsigned int, "stmt1"        , "statement 1 for mutation ops");
OPTION( Stmt2       , unsigned int, "stmt2"        , "statement 2 for mutation ops");
OPTION( Value1      , std::string , "value1"       , "string value for mutation ops");
OPTION( Value2      , std::string , "value2"       , "second string value for mutation ops");
OPTION( File1       , std::string , "file1"        , "file containing value1");
OPTION( File2       , std::string , "file2"        , "file containing value2");
OPTION( Fields      , std::string , "fields"       , "comma-delimited list of JSON fields to output");
OPTION( Aux         , std::string , "aux"          , "comma-delimited list of auxiliary JSON entry kinds to output");
OPTION( Silent      , bool        , "silent"       , "do not print prompts in interactive mode");
OPTION( CtrlChar    , bool        , "ctrl"         , "print a control character after output in the interactive mode");
OPTION( Binary      , std::string , "binary"       , "binary with DWARF information for line->address mapping");
OPTION( DwarfFilepathMap, std::string, "dwarf-filepath-mapping", "mapping of filepaths used in compilation -> new filepath");

std::ostringstream MutateCmd;

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

        if (!Binary.empty()) {
            MutateCmd << "binary 0 " << Binary
                      << " " << DwarfFilepathMap << std::endl;
        }
        
        if (!File1.empty()) {
            Value1 = Utils::filenameToContents(File1);
        }
        if (!File2.empty()) {
            Value2 = Utils::filenameToContents(File2);
        }

        if (Number) {
            MutateCmd << "number 0" << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (NumberF) {
            MutateCmd << "number-full 0" << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Ids) {
            MutateCmd << "ids 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Annotate) {
            MutateCmd << "annotate 0" << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }

        if (List) {
            MutateCmd << "list 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }

        if (Json) {
            if (Stmt1) {
                MutateCmd << "echo [" << std::endl;
                MutateCmd << "ast 0." << Stmt1;
            }
            else {
                MutateCmd << "json 0";
            }
            if (!Aux.empty()) {
                std::string sep = "";
                MutateCmd << " aux=";
                for (auto & aux : Utils::split(Aux, ',')) {
                    MutateCmd << sep << aux;
                    sep = ",";
                }
            }
            if (!Fields.empty()) {
                std::string sep = "";
                MutateCmd << " fields=";
                for (auto & key : Utils::split(Fields, ',')) {
                    MutateCmd << sep << key;
                    sep = ",";
                }
            }
            MutateCmd << std::endl;
            if (Stmt1) {
                MutateCmd << "echo ]" << std::endl;
            }
            return clang_mutate::CreateTU(CI);
        }
        if (Cut) {
            MutateCmd << "cut 0." << Stmt1 << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (SetRange) {
            MutateCmd << "set-range "
                      << "0." << Stmt1 << " "
                      << "0." << Stmt2 << " "
                      << Utils::escape(Value1) << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (SetFunc) {
            MutateCmd << "set-func "
                      << "0." << Stmt1 << " "
                      << Utils::escape(Value1) << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Insert) {
            MutateCmd << "get    0." << Stmt1 << " as $stmt" << std::endl
                      << "insert 0." << Stmt2 << " $stmt" << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Swap) {
            MutateCmd << "swap 0." << Stmt1
                      << " 0." << Stmt2 << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Set) {
            MutateCmd << "set 0." << Stmt1 << " "
                      << Utils::escape(Value1) << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Set2) {
            MutateCmd << "set "
                      << "0." << Stmt1 << " " << Utils::escape(Value1) << " "
                      << "0." << Stmt2 << " " << Utils::escape(Value2)
                      << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (InsertV) {
            MutateCmd << "insert 0." << Stmt1 << " "
                      << Utils::escape(Value1) << std::endl
                      << "preview 0" << std::endl;
            return clang_mutate::CreateTU(CI);
        }
        if (Interactive) {
            return clang_mutate::CreateTU(CI);
        }
        
        errs() << "Must supply one of:\n";
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
        errs() << "\tinteractive\n";
        
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
    int result = Tool.run(newFAF<ActionFactory>(&Factory, &Factory).get());

    if (Interactive) {
        clang_mutate::interactive_flags["ctrl"  ] = CtrlChar;
        clang_mutate::interactive_flags["prompt"] = !Silent;
        clang_mutate::runInteractiveSession(std::cin);
    }
    else {
        clang_mutate::interactive_flags["ctrl"  ] = false;
        clang_mutate::interactive_flags["prompt"] = false;
        std::istringstream cmd(MutateCmd.str());
        clang_mutate::runInteractiveSession(cmd);
    }
    
    return result;
}
