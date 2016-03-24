
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
