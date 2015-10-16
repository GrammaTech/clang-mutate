// Copyright (C) 2012 Eric Schulte                         -*- C++ -*-
#ifndef DRIVER_ASTMUTATORS_H
#define DRIVER_ASTMUTATORS_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

namespace clang {  

enum ACTION { NUMBER, IDS, ANNOTATOR, LISTER, CUT, INSERT, SWAP, GET, SET, VALUEINSERT };

ASTConsumer *CreateASTNumberer();
ASTConsumer *CreateASTIDS();
ASTConsumer *CreateASTAnnotator();
ASTConsumer *CreateASTLister();
ASTConsumer *CreateASTCuter(unsigned int Stmt);
ASTConsumer *CreateASTInserter(unsigned int Stmt1, unsigned int Stmt2);
ASTConsumer *CreateASTSwapper(unsigned int Stmt1, unsigned int Stmt2);
ASTConsumer *CreateASTGetter(unsigned int Stmt);
ASTConsumer *CreateASTSetter(unsigned int Stmt, StringRef Value);
ASTConsumer *CreateASTValueInserter(unsigned int Stmt, StringRef Value);
ASTConsumer *CreateASTBinaryAddressLister(StringRef Binary);

}

#endif
