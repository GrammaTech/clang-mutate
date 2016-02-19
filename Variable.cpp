#include "Variable.h"

#include <sstream>

using namespace clang;

VariableInfo::VariableInfo(
    const IdentifierInfo * _ident,
    size_t _index)
{
    ident = _ident;
    index = _index;
    name = ident->getName().str();
}

picojson::value VariableInfo::toJSON() const
{
    std::vector<picojson::value> ans;
    std::ostringstream oss;
    oss << "(|" << name << "|)";    
    ans.push_back(to_json(oss.str()));
    ans.push_back(to_json(index));
    return to_json(ans);
}
