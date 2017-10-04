
#include "Bindings.h"
#include "Macros.h"
#include "TypeDBEntry.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

#include <functional>
#include <set>
#include <map>

namespace clang_mutate {
using namespace clang;

void BindingCtx::push(const std::string & name,
                      Id id,
                      Hash type_hash)
{
  IdStack & bindings = ctx[name];
  bindings.push(std::make_pair(id, type_hash));
}

void BindingCtx::pop(const std::string & name)
{
  Context::iterator search = ctx.find(name);
  assert (search != ctx.end() && !search->second.empty());
  search->second.pop();
}
  
std::pair<BindingCtx::Id, Hash>
BindingCtx::lookup(const std::string & name) const
{
  Context::const_iterator search = ctx.find(name);
  return (search == ctx.end() || search->second.empty())
      ? std::pair<Id,Hash>(NULL, 0)
      : search->second.top();
}

bool BindingCtx::is_bound(const std::string & name) const
{
  return lookup(name).first != NULL;
}

bool GetBindingCtx::TraverseVarDecl(VarDecl * decl)
{
  std::string name = decl->getQualifiedNameAsString();
  IdentifierInfo * ident = decl->getIdentifier();
  Hash type_hash = hash_type(decl, ci, context);

  ctx.push(name, ident, type_hash);

  bool cont = Base::TraverseVarDecl(decl);
  return cont;
}

std::set<Hash> GetBindingCtx::required_types() const
{
    std::set<Hash> ans = ctx.required_types();
    for (std::set<Hash>::const_iterator it = addl_types.begin();
         it != addl_types.end();
         ++it)
    {
        ans.insert(*it);
    }
    return ans;
}

std::set<std::string> GetBindingCtx::required_includes() const
{ return includes; }

std::set<Hash> BindingCtx::required_types() const
{
    std::set<Hash> ans;
    for (std::map<std::string, IdStack>::const_iterator
             it = ctx.begin(); it != ctx.end(); ++it)
    {
        if (it->second.empty())
            continue;
        Hash type_hash = it->second.top().second;
        if (type_hash != 0)
            ans.insert(type_hash);
    }
    return ans;
}

bool GetBindingCtx::VisitStmt(Stmt * expr)
{
  if (isa<DeclRefExpr>(expr)) {
    DeclRefExpr * declref = static_cast<DeclRefExpr*>(expr);
    ValueDecl * vdecl     = declref->getDecl();
    std::string name      = vdecl->getQualifiedNameAsString();
    IdentifierInfo * id   = vdecl->getIdentifier();
    bool isMacro = Utils::contains_macro(expr, ci->getSourceManager()) &&
                   vdecl->getLocStart().isMacroID();

    if (id != NULL && !ctx.is_bound(name) && !isMacro) {
        std::string header;
        if (Utils::in_header(vdecl->getLocation(), ci, header)) {
            includes.insert(header);
        }
        else if (isa<FunctionDecl>(vdecl)) {
            unbound_f.insert(
                FunctionInfo(static_cast<FunctionDecl*>(vdecl)));
        }
        else {
            unbound_v.insert(BindingCtx::Binding(name, id));
        }
    }
  }
  return true;
}

void GetBindingCtx::addAddlType(const QualType & qt)
{
    Hash type_hash = hash_type(qt, ci, context);
    if (type_hash != 0)
        addl_types.insert(type_hash);
}

bool GetBindingCtx::VisitUnaryExprOrTypeTraitExpr(
    UnaryExprOrTypeTraitExpr * expr)
{
    if (expr->isArgumentType())
        addAddlType(expr->getArgumentType());
    return true;
}

bool GetBindingCtx::VisitExplicitCastExpr(
    ExplicitCastExpr * expr)
{
    addAddlType(expr->getTypeAsWritten());
    return true;
}

} // end namespace clang_mutate
