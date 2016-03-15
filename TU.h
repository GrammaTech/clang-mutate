#ifndef CLANG_MUTATE_TU_H
#define CLANG_MUTATE_TU_H

#include "AST.h"
#include "Scopes.h"
#include "BinaryAddressMap.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"

namespace clang_mutate {
    
struct TU
{
    TU(clang::CompilerInstance * _ci) : ci(_ci), astTable(), addrMap() {}
    clang::CompilerInstance * ci;
    AstTable astTable;
    BinaryAddressMap addrMap;
    Scope scopes;
};

extern std::vector<TU> TUs;

std::unique_ptr<clang::ASTConsumer>
CreateTU(clang::StringRef Binary,
         clang::StringRef DwarfFilepathMap,
         clang::CompilerInstance * CI);

} // end namespace clang_mutate

#endif