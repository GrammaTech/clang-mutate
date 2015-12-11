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

// Enable the following macro to rewrite free function
// names; without this macro, function names will just
// be "rewritten" to themselves, making it hard to
// do substitution.
//#define ALLOW_FREE_FUNCTIONS

enum RenameKind
{
    FunctionRename,
    VariableRename
};

struct RenameDatum
{
    RenameDatum(const clang::IdentifierInfo* id,
                const std::string & n,
                size_t i,
                RenameKind k )
    : ident(id), name(n), index(i), kind(k) {}
    
    const clang::IdentifierInfo * ident;
    std::string name;
    size_t index;
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

RenameDatum mkVariableRename(
    clang::IdentifierInfo * id,
    const std::string & name,
    size_t index);

RenameDatum mkFunctionRename(
    clang::IdentifierInfo * id,
    const std::string & name);

typedef std::set<RenameDatum> Renames;

Renames make_renames(
    const std::set<std::pair<clang::IdentifierInfo*, size_t> > & free_vars,
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
  clang::SourceLocation begin;
  clang::SourceLocation end;
  typedef std::map<size_t, std::pair<size_t, std::string> > RewriteMap;
  RewriteMap rewrites;
};

#endif
