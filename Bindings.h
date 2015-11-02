#ifndef AST_BINDINGS_H
#define AST_BINDINGS_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <string>
#include <stack>
#include <map>
#include <set>

namespace clang_mutate {

class BindingCtx
{
public:

  typedef clang::IdentifierInfo* Id;
  typedef std::stack<Id> IdStack;
  typedef std::map<std::string, IdStack> Context;
  typedef std::pair<std::string, Id> Binding;
  
  BindingCtx() : ctx() {}

  void push(const std::string & name, Id id);
  void pop(const std::string & name);
  Id lookup(const std::string & name) const;
  bool is_bound(const std::string & name) const;
  
private:
  Context ctx;
};
  
class GetBindingCtx
  : public clang::ASTConsumer
  , public clang::RecursiveASTVisitor<GetBindingCtx>
{
  typedef RecursiveASTVisitor<GetBindingCtx> Base;

public:

  GetBindingCtx() : ctx() {}

  GetBindingCtx(const BindingCtx & _ctx) : ctx(_ctx) {}

  GetBindingCtx(const GetBindingCtx & b) : ctx(b.ctx) {}
  
  bool TraverseVarDecl(clang::VarDecl * decl);

  bool VisitStmt(clang::Stmt * stmt);

  std::set<clang::IdentifierInfo*> free_values() const;
  std::set<clang::IdentifierInfo*> free_functions() const;
  
  void dump() const;
  
private:
  BindingCtx ctx;
  std::set<BindingCtx::Binding> unbound_v, unbound_f;
};
 
} // end namespace clang_mutate

#endif
