#ifndef DRIVER_ASTMUTATORS_H
#define DRIVER_ASTMUTATORS_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

namespace clang_mutate {  

clang::ASTConsumer *CreateASTBinaryAddressLister(clang::StringRef Binary);

}

#endif
