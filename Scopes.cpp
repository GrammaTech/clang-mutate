
#include "Scopes.h"

namespace clang_mutate {
using namespace clang;

DeclScope::DeclScope()
{
    enter_scope();
}
    
void DeclScope::declare(const IdentifierInfo* id)
{
    if (id != NULL)
        scopes.back().push_back(id);
}
    
void DeclScope::enter_scope()
{
    std::vector<const IdentifierInfo*> empty;
    scopes.push_back(empty);
}

void DeclScope::exit_scope()
{
    scopes.pop_back();
}

std::vector<std::string> DeclScope::get_names_in_scope(size_t length) const
{
    std::vector<std::string> ans;
    if (length == 0)
        return ans;
    typedef std::vector<const IdentifierInfo*> Ids;
    std::vector<Ids>::const_reverse_iterator s;
    Ids::const_reverse_iterator v;
    for (s = scopes.rbegin(); s != scopes.rend(); ++s) {
        for (v = s->rbegin(); v != s->rend(); ++v) {
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
    return (isa<CompoundStmt>(stmt) ||
            isa<IfStmt>(stmt)       );
}

} // end namespace clang_mutate
