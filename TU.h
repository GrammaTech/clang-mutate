#ifndef CLANG_MUTATE_TU_H
#define CLANG_MUTATE_TU_H

#include "Ast.h"
#include "Scopes.h"
#include "BinaryAddressMap.h"
#include "LLVMInstructionMap.h"
#include "Json.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"

#include <map>
#include <vector>

namespace clang_mutate {
    
struct TU
{
    TU(TURef _tuid)
      : tuid(_tuid)
      , asts()
      , addrMap()
      , llvmInstrMap()
    {}
    ~TU();
    TURef tuid;
    bool allowDeclAsts;
    std::vector<Ast*> asts;
    BinaryAddressMap addrMap;
    LLVMInstructionMap llvmInstrMap;
    std::string source;
    Scope scopes;
    std::map<std::string, std::vector<picojson::value> > aux;
    std::map<AstRef, SourceOffset> function_starts;
    std::string filename;

    AstRef nextAstRef() const;
};

extern std::map<TURef, TU*> TUs;

extern TU * tu_in_progress;
std::unique_ptr<clang::ASTConsumer>
CreateTU(clang::CompilerInstance * CI, bool WithCfg=false);

} // end namespace clang_mutate

#endif
