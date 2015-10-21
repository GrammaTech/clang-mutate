// Copyright (C) 2012 Eric Schulte                         -*- C++ -*-
#ifndef DRIVER_ASTMUTATORS_H
#define DRIVER_ASTMUTATORS_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

namespace clang_mutate {  

enum ACTION { NUMBER, IDS, ANNOTATOR, LISTER, CUT, INSERT, SWAP, GET, SET, VALUEINSERT };

clang::ASTConsumer *CreateASTNumberer();
clang::ASTConsumer *CreateASTIDS();
clang::ASTConsumer *CreateASTAnnotator();
clang::ASTConsumer *CreateASTLister();
clang::ASTConsumer *CreateASTCuter(unsigned int Stmt);
clang::ASTConsumer *CreateASTInserter(unsigned int Stmt1, unsigned int Stmt2);
clang::ASTConsumer *CreateASTSwapper(unsigned int Stmt1, unsigned int Stmt2);
clang::ASTConsumer *CreateASTGetter(unsigned int Stmt);
clang::ASTConsumer *CreateASTSetter(unsigned int Stmt, clang::StringRef Value);
clang::ASTConsumer *CreateASTValueInserter(unsigned int Stmt, clang::StringRef Value);
clang::ASTConsumer *CreateASTBinaryAddressLister(clang::StringRef Binary);

}

#endif
