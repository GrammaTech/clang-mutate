#include "Function.h"

#include <sstream>

using namespace clang;

FunctionInfo::FunctionInfo(FunctionDecl * decl)
{
    name = decl->getQualifiedNameAsString();
    ident = decl->getIdentifier();
    const Type * rt = decl->getReturnType().getTypePtrOrNull();
    returns_void = rt == NULL || rt->isVoidType();
    num_params = decl->getNumParams();
    is_variadic = decl->isVariadic();    
}

picojson::value FunctionInfo::toJSON() const
{
    std::vector<picojson::value> ans;
    std::ostringstream oss;
    oss << "(|" << name << "|)";
    ans.push_back(to_json(oss.str()));
    ans.push_back(to_json(returns_void));
    ans.push_back(to_json(is_variadic));
    ans.push_back(to_json(num_params));
    return to_json(ans);
}
