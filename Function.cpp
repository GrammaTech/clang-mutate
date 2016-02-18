#include "Function.h"

using namespace clang;

FunctionInfo::FunctionInfo(FunctionDecl * decl)
{
    name = decl->getQualifiedNameAsString();
    id = decl->getIdentifier();
    const Type * rt = decl->getReturnType().getTypePtrOrNull();
    returns_void = rt == NULL || rt->isVoidType();
    num_params = decl->getNumParams();
    is_variadic = decl->isVariadic();    
}

picojson::value FunctionInfo::toJSON() const
{
    std::vector<picojson::value> ans;
    ans.push_back(to_json(name));
    ans.push_back(to_json(returns_void));
    ans.push_back(to_json(is_variadic));
    ans.push_back(to_json(num_params));
    return to_json(ans);
}
