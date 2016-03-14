
#include "Scopes.h"
#include "Utils.h"

namespace clang_mutate {
using namespace clang;

Scope::Scope()
{ enter_scope(NoAst); }

void Scope::enter_scope(AstRef scope)
{ scopes.push(ScopeInfo(scope)); }

void Scope::exit_scope()
{
    AstRef _;
    // Rewind past declarations in this scope.
    while (!scopes.isEmpty() && !scopes.pt().getScope(_))
        scopes.pop();
    // Rewind the scope itself.
    if (!scopes.isEmpty())
        scopes.pop();
}

void Scope::declare(const IdentifierInfo * id)
{
    if (id != NULL)
        scopes.push(ScopeInfo(id));
}

PTNode Scope::current_scope_position() const
{ return scopes.position(); }

void Scope::restore_scope_position(PTNode pos)
{ scopes.moveTo(pos); }
    
AstRef Scope::get_enclosing_scope(PTNode pos) const
{
    for (auto decl : scopes.spineFrom(pos)) {
        AstRef scope;
        if (decl.getScope(scope))
            return scope;
    }
    return NoAst;
}

std::vector<std::vector<std::string> >
Scope::get_names_in_scope_from(PTNode pos, size_t length) const
{
    std::vector<std::vector<std::string> > ans;
    std::vector<std::string> scope;
    for (auto decl : scopes.spineFrom(pos)) {
        std::string name;
        if (decl.getDeclaration(name)) {
            if (length > 0) {
                scope.push_back(name);
                --length;
            }
        }
        else {
            ans.push_back(scope);
            scope.clear();
        }
    }
    return ans;
}

std::vector<std::vector<std::string> >
Scope::get_names_in_scope() const
{
    std::vector<std::vector<std::string> > ans;
    std::vector<std::string> scope;
    for (auto decl : scopes.spine()) {
        std::string name;
        if (decl.getDeclaration(name)) {
            scope.push_back(name);
        }
        else {
            ans.push_back(scope);
            scope.clear();
        }
    }
    return ans;
}

DeclScope::DeclScope()
{
    // Is there some top-level IdentifierInfo* that could be used
    // here instead of NULL?
    enter_scope(NULL);
}
    
void DeclScope::declare(const IdentifierInfo* id)
{
    if (id != NULL)
        scopes.back().second.push_back(id);
}
    
void DeclScope::enter_scope(Stmt * stmt)
{
    std::vector<const IdentifierInfo*> empty;
    scopes.push_back(std::make_pair(stmt, empty));
}

void DeclScope::exit_scope()
{
    scopes.pop_back();
}

Stmt * DeclScope::current_scope() const
{
    return scopes.rbegin()->first;
}

std::vector<std::vector<std::string> >
DeclScope::get_names_in_scope(size_t length) const
{
    std::vector<std::vector<std::string> > ans;
    if (length == 0)
        return ans;
    std::vector<BlockScope>::const_reverse_iterator s;
    std::vector<const IdentifierInfo*>::const_reverse_iterator v;
    for (s = scopes.rbegin(); s != scopes.rend(); ++s) {
        ans.push_back(std::vector<std::string>());
        if (length == 0)
            continue; // empty scopes if we hit the length limit.
        for (v = s->second.rbegin(); v != s->second.rend(); ++v) {
            ans.back().push_back((*v)->getName().str());
            if (--length == 0)
                break;
        }
    }
    return ans;
}

bool begins_scope(Stmt * stmt)
{
    if (stmt == NULL)
        return false;
    return (isa<CompoundStmt>(stmt));
}

bool begins_pseudoscope(Decl * decl)
{
    if (decl == NULL)
        return false;
    return ( isa<BlockDecl>(decl)    ||
             isa<CapturedDecl>(decl) ||
             isa<FunctionDecl>(decl) );
}

} // end namespace clang_mutate
