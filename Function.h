#ifndef CLANG_MUTATE_FUNCTION_H
#define CLANG_MUTATE_FUNCTION_H

#include "Utils.h"

#include "clang/AST/AST.h"
#include <string>

class FunctionInfo
{
public:
    FunctionInfo(clang::FunctionDecl * decl);

    FunctionInfo(const FunctionInfo & that)
        : name(that.name)
        , id(that.id)
        , returns_void(that.returns_void)
        , is_variadic(that.is_variadic)
        , num_params(that.num_params)
    {}

    clang::IdentifierInfo * getId() const { return id; }
    
    bool operator==(const FunctionInfo & that) const
    { return (id == that.id); }
    
    bool operator<(const FunctionInfo & that) const
    { return (id < that.id); }

    picojson::value toJSON() const;
    
private:
    std::string name;
    clang::IdentifierInfo * id;
    bool returns_void;
    bool is_variadic;
    unsigned int num_params;
};

template <> inline
picojson::value to_json(const FunctionInfo & f)
{ return f.toJSON(); }

#endif
