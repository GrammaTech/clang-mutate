// Copyright (C) 2012 Eric Schulte                         -*- C++ -*-
#ifndef DRIVER_ASTMUTATORS_H
#define DRIVER_ASTMUTATORS_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

namespace clang_mutate {  

enum ACTION { NUMBER
            , IDS
            , ANNOTATOR
            , CUT
            , CUTENCLOSING
            , INSERT
            , SWAP
            , GET
            , SET
            , SET2
            , SETRANGE
            , VALUEINSERT
            , PREINSERT
            , GETINFO
            , GETSCOPE
            };

std::unique_ptr<clang::ASTConsumer> CreateASTNumberer();
std::unique_ptr<clang::ASTConsumer> CreateASTIDS();
std::unique_ptr<clang::ASTConsumer> CreateASTAnnotator();
std::unique_ptr<clang::ASTConsumer> CreateASTCutter(unsigned int Stmt);
std::unique_ptr<clang::ASTConsumer> CreateASTRangeSetter(unsigned int Stmt1, unsigned int Stmt2, clang::StringRef Value);
std::unique_ptr<clang::ASTConsumer> CreateASTEnclosingCutter(unsigned int Stmt);
std::unique_ptr<clang::ASTConsumer> CreateASTInserter(unsigned int Stmt1, unsigned int Stmt2);
std::unique_ptr<clang::ASTConsumer> CreateASTSwapper(unsigned int Stmt1, unsigned int Stmt2);
std::unique_ptr<clang::ASTConsumer> CreateASTGetter(unsigned int Stmt);
std::unique_ptr<clang::ASTConsumer> CreateASTSetter(unsigned int Stmt, clang::StringRef Value);
std::unique_ptr<clang::ASTConsumer> CreateASTSetter2(unsigned int Stmt1, clang::StringRef Value1,
                                                     unsigned int Stmt2, clang::StringRef Value2);
std::unique_ptr<clang::ASTConsumer> CreateASTValueInserter(unsigned int Stmt, clang::StringRef Value);
std::unique_ptr<clang::ASTConsumer> CreateASTValuePreInserter(unsigned int Stmt, clang::StringRef Value);
std::unique_ptr<clang::ASTConsumer> CreateASTInfoGetter(unsigned int Stmt);
std::unique_ptr<clang::ASTConsumer> CreateASTScopeGetter(unsigned int Stmt, unsigned int Depth);

}

#endif
