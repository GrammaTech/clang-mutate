#ifndef RENAMING_H
#define RENAMING_H

#include "Json.h"
#include "Function.h"
#include "Variable.h"

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

class Replacements
{
public:
    void add(size_t offset, const std::string & old_str, const std::string & new_str);
    std::string apply_to(const std::string & input) const;
private:
    std::map<size_t, std::pair<std::string, std::string> > replacements;
};

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

typedef std::pair<std::set<VariableInfo>,
                  std::set<FunctionInfo> > Renames;

class RenameFreeVar
  : public clang::RecursiveASTVisitor<RenameFreeVar>
  , public clang::ASTConsumer
{
public:
  typedef clang::RecursiveASTVisitor<RenameFreeVar> Base;
  
  RenameFreeVar(clang::Stmt * the_stmt,
                clang::SourceManager & _sm,
                const clang::LangOptions & _langOpts,
                clang::SourceLocation _begin,
                clang::SourceLocation _end,
                const Renames & renames);

  RenameFreeVar(clang::Decl * the_decl,
                clang::SourceManager & _sm,
                const clang::LangOptions & _langOpts,
                clang::SourceLocation _begin,
                clang::SourceLocation _end,
                const Renames & renames);

  bool VisitStmt(clang::Stmt * stmt);

  std::string getRewrittenString() const;

  std::set<std::string> getIncludes() const;

  Replacements getReplacements() const
  { return replacements; }
  
private:
  clang::SourceManager & sm;
  const clang::LangOptions & langOpts;
  clang::SourceLocation begin;
  clang::SourceLocation end;
  const Renames & renames;
  Replacements replacements;
  std::set<std::string> includes;
};

#endif
