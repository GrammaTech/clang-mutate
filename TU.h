#ifndef CLANG_MUTATE_TU_H
#define CLANG_MUTATE_TU_H

#include "AST.h"
#include "Scopes.h"
#include "BinaryAddressMap.h"

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

};

#endif
