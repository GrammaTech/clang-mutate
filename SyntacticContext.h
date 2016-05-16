#ifndef CLANG_MUTATE_SYNTACTIC_CONTEXT_H
#define CLANG_MUTATE_SYNTACTIC_CONTEXT_H

#include "Json.h"

namespace clang_mutate {

struct SyntacticContext
{
    typedef enum {
        Before,
        Instead,
        After
    } Where;
    
    typedef enum {
        #define CONTEXT(name, before, instead, after) name ## _k,
        #include "SyntacticContext.cxx"
    } Kind;

    #define CONTEXT(name, before, instead, after) \
        static SyntacticContext name() { return SyntacticContext(name ## _k); }
    #include "SyntacticContext.cxx"
        
    static std::string adjust(std::string text,
                              SyntacticContext sctx,
                              Where where);

    SyntacticContext & operator=(const SyntacticContext & that)
    { m_kind = that.m_kind; return *this; }
        
    bool operator==(const SyntacticContext & that) const
    { return this->m_kind == that.m_kind; }

    bool operator<(const SyntacticContext & that) const
    { return this->m_kind < that.m_kind; }

    picojson::value toJSON() const;
private:
    SyntacticContext(Kind k) : m_kind(k) {}
    Kind m_kind;
};

} // end namespace clang_mutate

template <> inline
picojson::value to_json(const clang_mutate::SyntacticContext & sctx)
{ return sctx.toJSON(); }

template <>
struct describe_json<clang_mutate::SyntacticContext>
{ static std::string str() { return "syntactic-context"; } };

#endif
