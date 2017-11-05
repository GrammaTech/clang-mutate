
#include "Bindings.h"
#include "Macros.h"
#include "TypeDBEntry.h"

#include "clang/Lex/Lexer.h"

#include <iostream>

#include <algorithm>
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

  types.push_back(type_hash);
  bindings.push(std::make_pair(id, type_hash));
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

std::vector<Hash> GetBindingCtx::required_types() const
{
    std::vector<Hash> ans;
    std::vector<Hash> required_types = ctx.required_types();

    std::copy(required_types.begin(),
              required_types.end(),
              std::back_inserter(ans));
    std::copy(addl_types.begin(),
              addl_types.end(),
              std::back_inserter(ans));
    ans.erase(std::unique(ans.begin(), ans.end()), ans.end());

    return ans;
}

std::set<std::string> GetBindingCtx::required_includes() const
{ return includes; }

std::vector<Hash> BindingCtx::required_types() const
{
    std::vector<Hash> ans(types);
    ans.erase(std::unique(ans.begin(), ans.end()), ans.end());
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
        addl_types.push_back(type_hash);
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
