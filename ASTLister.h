#ifndef AST_LISTER_H
#define AST_LISTER_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"

namespace clang_mutate {  

std::unique_ptr<clang::ASTConsumer>
CreateASTLister(clang::StringRef Binary,
                bool OutputAsJSON,
                clang::CompilerInstance * CI);

}

#endif
