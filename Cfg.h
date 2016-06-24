#ifndef __CFG_H
#define __CFG_H

#include "TU.h"
#include <clang/AST/ASTContext.h>

namespace clang_mutate {

// Add control flow successors to ASTs in TU.
void GenerateCFG(TU &TU, clang::CompilerInstance const &CI, clang::ASTContext &Context);

} // namespace clang_mutate


#endif // __CFG_H
