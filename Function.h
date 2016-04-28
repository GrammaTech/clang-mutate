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
        , ident(that.ident)
        , returns_void(that.returns_void)
        , is_variadic(that.is_variadic)
        , num_params(that.num_params)
    {}

    const clang::IdentifierInfo * getId() const { return ident; }

    std::string getName() const { return name; }
    
    bool operator<(const FunctionInfo & that) const
    { return ident < that.ident; }

    picojson::value toJSON() const;
    
private:
    std::string name;
    const clang::IdentifierInfo * ident;
    bool returns_void;
    bool is_variadic;
    unsigned int num_params;
};

template <> inline
picojson::value to_json(const FunctionInfo & f)
{ return f.toJSON(); }

template <>
struct describe_json<FunctionInfo>
{
    static std::string str()
    { return "[\"replacement_name\", returns_void?"
             ", is_variadic?, num_params]";
    }
};

#endif
