#include "Variable.h"

#include <sstream>

using namespace clang;

VariableInfo::VariableInfo(const IdentifierInfo * _ident)
{
    ident = _ident;
    name = ident->getName().str();
}

picojson::value VariableInfo::toJSON() const
{
    std::ostringstream oss;
    oss << "(|" << name << "|)";    
    return to_json(oss.str());
}
