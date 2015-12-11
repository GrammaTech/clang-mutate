
#include "Scopes.h"

namespace clang_mutate {
using namespace clang;

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

std::vector<std::string> DeclScope::get_names_in_scope(size_t length) const
{
    std::vector<std::string> ans;
    if (length == 0)
        return ans;
    std::vector<BlockScope>::const_reverse_iterator s;
    std::vector<const IdentifierInfo*>::const_reverse_iterator v;
    for (s = scopes.rbegin(); s != scopes.rend(); ++s) {
        for (v = s->second.rbegin(); v != s->second.rend(); ++v) {
            ans.push_back((*v)->getName().str());
            if (--length == 0)
                return ans;
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

} // end namespace clang_mutate
