#ifndef CLANG_MUTATE_AST_REF_H
#define CLANG_MUTATE_AST_REF_H

#include <stddef.h>
#include "Json.h"

#include <iostream>

namespace clang_mutate {

typedef size_t TURef;
typedef size_t AstCounter;

struct TU;
class Ast;

struct AstRef
{
    AstRef() : m_tu(0), m_index(0) {}
    AstRef(const AstRef & that)
      : m_tu(that.m_tu)
      , m_index(that.m_index)
    {}
    AstRef(TURef tu, AstCounter index)
      : m_tu(tu)
      , m_index(index)
    {}

    AstRef& operator=(const AstRef & that)
    {
        m_tu = that.m_tu;
        m_index = that.m_index;
        return *this;
    }

    bool operator==(const AstRef & x) const
    { return m_tu == x.m_tu && m_index == x.m_index; }

    bool operator!=(const AstRef & x) const
    { return !(*this == x); }

    bool operator<(const AstRef & x) const
    {
        return  m_tu < x.m_tu
            || (m_tu == x.m_tu && m_index < x.m_index);
    }

    bool operator>(const AstRef & x) const
    {
        return  m_tu > x.m_tu
            || (m_tu == x.m_tu && m_index > x.m_index);
    }

    bool operator<=(const AstRef & x) const
    { return !(*this > x); }

    bool operator>=(const AstRef & x) const
    { return !(*this < x); }

    TU & tu() const;
    TURef tuid() const { return m_tu; }

    Ast * operator->() const;
    Ast & operator*() const;

    std::string to_string() const;

    AstCounter counter() const { return m_index; }
private:
    TURef m_tu;
    AstCounter m_index;
};

extern const AstRef NoAst;

} // end namespace clang_mutate

inline std::ostream & operator<<(std::ostream & o,
                                 const clang_mutate::AstRef & astref)
{ o << astref.to_string(); return o; }

template <> inline
picojson::value to_json(const clang_mutate::AstRef & ref)
{ return to_json(ref.counter()); }

#endif
