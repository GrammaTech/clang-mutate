#ifndef RENAMING_H
#define RENAMING_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <string>
#include <set>
#include <map>

enum RenameKind
{
    FunctionRename,
    VariableRename
};

struct RenameDatum
{
    RenameDatum(const clang::IdentifierInfo* id,
                const std::string & n,
                RenameKind k)
    : ident(id), name(n), kind(k) {}
    
    const clang::IdentifierInfo * ident;
    std::string name;
    RenameKind kind;

    bool operator<(const RenameDatum & that) const
    {
        if (ident == NULL && that.ident != NULL)
            return true;
        if (ident != NULL && that.ident == NULL)
            return false;
        if (ident == NULL && that.ident == NULL)
            return name < that.name;
        return ident < that.ident;
    }
};

typedef std::set<RenameDatum> Renames;

Renames make_renames(const std::set<clang::IdentifierInfo*> & free_vars,
                     const std::set<clang::IdentifierInfo*> & free_funs);

class RenameFreeVar
  : public clang::RecursiveASTVisitor<RenameFreeVar>
  , public clang::ASTConsumer
{
public:
  typedef clang::RecursiveASTVisitor<RenameFreeVar> Base;
  
  RenameFreeVar(clang::Stmt * the_stmt,
                clang::Rewriter & r,
                const Renames & renames);
  
  bool VisitStmt(clang::Stmt * stmt);

  std::string getRewrittenString() const;
  
private:
  clang::Rewriter & rewriter;
  const Renames & renames;
  const char * begin;
  const char * end;
  std::map<const char*, std::pair<size_t, std::string> > rewrites;
};

#endif
