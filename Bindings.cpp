
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

  const Type * tdecl = decl->getTypeSourceInfo()->getType().getTypePtrOrNull();
  Hash type_hash = hash_type(tdecl, ci);

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
    if (id != NULL && !ctx.is_bound(name)) {
      if (isa<FunctionDecl>(vdecl)) {
        std::string header;
        if (Utils::in_system_header(vdecl->getLocation(),
                                     ci->getSourceManager(),
                                     header))
        {
          includes.insert(header);
        }
        else
        {
          unbound_f.insert(BindingCtx::Binding(name, id));
        }
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
    Hash type_hash = hash_type(qt.getTypePtrOrNull(), ci);
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

static void print_unbound(const std::set<BindingCtx::Binding> & unbound)
{
  std::cerr << "{ ";
  for (std::set<BindingCtx::Binding>::iterator it = unbound.begin();
       it != unbound.end();
       ++it)
  {
    std::cerr << it->first << " ";
  }
  std::cerr << "}" << std::endl;
}
  
void GetBindingCtx::dump() const
{
  std::cerr << "unbound value identifiers: ";
  print_unbound(unbound_v);
  
  std::cerr << "unbound function identifiers: ";
  print_unbound(unbound_f);
}

std::set<std::pair<IdentifierInfo*, size_t> > GetBindingCtx::free_values(
    const std::vector<std::set<IdentifierInfo*> > & scopes) const
{
    std::set<std::pair<IdentifierInfo*, size_t> > ans;
    for (std::set<BindingCtx::Binding>::const_iterator it = unbound_v.begin();
         it != unbound_v.end();
         ++it)
    {
        size_t index = 0;
        typedef std::set<IdentifierInfo*> Ids;
        for (std::vector<Ids>::const_reverse_iterator scope = scopes.rbegin();
             scope != scopes.rend();
             ++scope)
        {
            if (scope->find(it->second) != scope->end())
                break;
            ++index;
        }
        ans.insert(std::make_pair(it->second, index));
    }
    return ans;
}

std::set<IdentifierInfo*> GetBindingCtx::free_functions() const
{
    std::set<IdentifierInfo*> ans;
    for (std::set<BindingCtx::Binding>::const_iterator it = unbound_f.begin();
         it != unbound_f.end();
         ++it)
    {
        ans.insert(it->second);
    }
    return ans;
}
  
} // end namespace clang_mutate
