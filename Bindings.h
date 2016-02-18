#ifndef AST_BINDINGS_H
#define AST_BINDINGS_H

#include "Utils.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/CompilerInstance.h"

#include <string>
#include <stack>
#include <map>
#include <set>

namespace clang_mutate {

struct BindingDatum
{
    clang::IdentifierInfo * id;
    Hash type_hash;
};

class BindingCtx
{
public:

  typedef clang::IdentifierInfo* Id;
  typedef std::stack<std::pair<Id, Hash> > IdStack;
  typedef std::map<std::string, IdStack> Context;
  typedef std::pair<std::string, Id> Binding;
  
  BindingCtx() : ctx() {}

  void push(const std::string & name, Id id, Hash type_hash);
  void pop(const std::string & name);
  std::pair<Id,Hash> lookup(const std::string & name) const;
  bool is_bound(const std::string & name) const;

  std::set<Hash> required_types() const;
  
private:
  Context ctx;
};
  
class GetBindingCtx
  : public clang::ASTConsumer
  , public clang::RecursiveASTVisitor<GetBindingCtx>
{
  typedef RecursiveASTVisitor<GetBindingCtx> Base;

public:

  GetBindingCtx(clang::CompilerInstance * _ci) : ctx(), ci(_ci) {}

  GetBindingCtx(const BindingCtx & _ctx,
                clang::CompilerInstance * _ci) : ctx(_ctx), ci(_ci) {}

  GetBindingCtx(const GetBindingCtx & b) : ctx(b.ctx), ci(b.ci) {}

  bool TraverseVarDecl(clang::VarDecl * decl);

  bool VisitStmt(clang::Stmt * stmt);

  bool VisitExplicitCastExpr(clang::ExplicitCastExpr * expr);
  bool VisitUnaryExprOrTypeTraitExpr(
      clang::UnaryExprOrTypeTraitExpr * expr);

  std::set<std::pair<clang::IdentifierInfo*, size_t> > free_values(
      const std::vector<std::set<clang::IdentifierInfo*> > & scopes) const;
  std::set<clang::IdentifierInfo*> free_functions() const;
  std::set<Hash> required_types() const;
  std::set<std::string> required_includes() const;
  
  void dump() const;
  
private:

  void addAddlType(const clang::QualType & qt);

  BindingCtx ctx;
  std::set<BindingCtx::Binding> unbound_v, unbound_f;
  std::set<Hash> addl_types;
  std::set<std::string> includes;
  clang::CompilerInstance * ci;
};
 
} // end namespace clang_mutate

#endif
