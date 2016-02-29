#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#include "AST.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"

#include <iostream>

namespace clang_mutate {  

std::unique_ptr<clang::ASTConsumer>
CreateInteractive(clang::StringRef Binary,
                  clang::StringRef DwarfFilepathMap,
                  clang::CompilerInstance * CI);

void runInteractiveSession(std::istream & input);

}

#endif
