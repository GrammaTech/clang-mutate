#ifndef CLANG_MUTATE_VARIABLE_H
#define CLANG_MUTATE_VARIABLE_H

#include "Utils.h"

#include "clang/AST/AST.h"
#include <string>

class VariableInfo
{
public:
    VariableInfo(const clang::IdentifierInfo * _ident,
                 size_t _index);

    VariableInfo(const VariableInfo & that)
        : ident(that.ident)
        , name(that.name)
        , index(that.index)
    {}

    const clang::IdentifierInfo * getId() const
    { return ident; }

    std::string getName() const
    { return name; }
    
    bool operator<(const VariableInfo & that) const
    { return ident < that.ident; }
    
    picojson::value toJSON() const;
    
private:
    const clang::IdentifierInfo * ident;
    std::string name;
    size_t index;
};

template <> inline
picojson::value to_json(const VariableInfo & v)
{ return v.toJSON(); }

#endif
