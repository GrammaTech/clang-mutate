#ifndef AST_LISTER_H
#define AST_LISTER_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

namespace clang_mutate {  

clang::ASTConsumer *CreateASTLister(clang::StringRef Binary, bool OutputAsJSON);

}

#endif
