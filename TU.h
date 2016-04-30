#ifndef CLANG_MUTATE_TU_H
#define CLANG_MUTATE_TU_H

#include "AST.h"
#include "Scopes.h"
#include "BinaryAddressMap.h"
#include "Json.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"

#include <map>
#include <vector>

namespace clang_mutate {
    
struct TU
{
    TU(clang::CompilerInstance * _ci, TURef _tuid)
      : ci(_ci)
      , tuid(_tuid)
      , asts()
      , addrMap()
    {}
    ~TU();
    clang::CompilerInstance * ci;
    TURef tuid;
    bool allowDeclAsts;
    std::vector<Ast*> asts;
    BinaryAddressMap addrMap;
    std::string source;
    Scope scopes;
    std::map<std::string, std::vector<picojson::value> > aux;
    std::map<AstRef, clang::SourceRange> function_ranges;

    AstRef nextAstRef() const;
};

extern std::vector<TU*> TUs;

std::unique_ptr<clang::ASTConsumer>
CreateTU(clang::CompilerInstance * CI, bool AllowDeclAsts);

} // end namespace clang_mutate

#endif
