
#include "Bindings.h"

#include <iostream>

namespace clang_mutate {
using namespace clang;

void BindingCtx::push(const std::string & name, Id id)
{
  IdStack & bindings = ctx[name];
  bindings.push(id);
}

void BindingCtx::pop(const std::string & name)
{
  Context::iterator search = ctx.find(name);
  assert (search != ctx.end() && !search->second.empty());
  search->second.pop();
}
  
BindingCtx::Id BindingCtx::lookup(const std::string & name) const
{
  Context::const_iterator search = ctx.find(name);
  return (search == ctx.end() || search->second.empty())
    ? NULL
    : search->second.top();
}

bool BindingCtx::is_bound(const std::string & name) const
{
  return lookup(name) != NULL;
}
  
bool GetBindingCtx::TraverseVarDecl(VarDecl * decl)
{
  std::string name = decl->getQualifiedNameAsString();
  IdentifierInfo * ident = decl->getIdentifier();
  ctx.push(name, ident);
  bool cont = Base::TraverseVarDecl(decl);
  return cont;
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
	unbound_f.insert(BindingCtx::Binding(name, id));
      }
      else {
	unbound_v.insert(BindingCtx::Binding(name, id));
      }
    }
  }
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

std::vector<std::string> GetBindingCtx::free_values() const
{
  std::vector<std::string> ans;
  for (std::set<BindingCtx::Binding>::const_iterator it = unbound_v.begin();
       it != unbound_v.end();
       ++it)
  {
    ans.push_back(it->first);
  }
  return ans;
}
  
std::vector<std::string> GetBindingCtx::free_functions() const
{
  std::vector<std::string> ans;
  for (std::set<BindingCtx::Binding>::const_iterator it = unbound_f.begin();
       it != unbound_f.end();
       ++it)
  {
    ans.push_back(it->first);
  }
  return ans;
}
  

} // end namespace clang_mutate
