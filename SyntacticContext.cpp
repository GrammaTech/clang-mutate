#include "SyntacticContext.h"
#include "Utils.h"

using namespace clang_mutate;

static void no_change(std::string & str) {}

static void trailing_comma(std::string & str)
{ if (str[str.size() - 1] != ',') str += ','; }

static void leading_comma(std::string & str)
{ if (str[0] != ',') str = "," + str; }

static void add_semicolon(std::string & str)
{ if (str[str.size() - 1] != ';') str += ';'; }

static void add_braces(std::string & str)
{
    if (str[0] != '{' || str[str.size() - 1] != '}')
        str = "{" + str + "}";
}

static void add_semicolon_if_unbraced(std::string & str)
{
    if (str[str.size() - 1] != '}' && str[str.size() - 1] != ';')
        str += ';';
}

std::string SyntacticContext::adjust(
    std::string text,
    SyntacticContext sctx,
    Where where)
{
    // Trim leading and trailing whitespace,
    // (TODO: and comments) from text
    text = Utils::trim(text);
    if (text.empty())
        return text;
    
    switch (where)
    {
    case Before: {
        switch (sctx.m_kind) {
        #define CONTEXT(name, before, instead, after) \
            case name ## _k:                          \
              before(text);                           \
              break;
        #include "SyntacticContext.cxx"
        }
    } break;
    case Instead: {
        switch (sctx.m_kind) {
        #define CONTEXT(name, before, instead, after) \
            case name ## _k:                          \
              instead(text);                          \
              break;
        #include "SyntacticContext.cxx"
        }
    } break;
    case After: {
        switch (sctx.m_kind) {
        #define CONTEXT(name, before, instead, after) \
            case name ## _k:                          \
              after(text);                            \
              break;
        #include "SyntacticContext.cxx"
        }
    } break;
    }
    return text;
}
    
picojson::value SyntacticContext::toJSON() const
{
    switch (m_kind) {
        #define CONTEXT(name, before, instead, after)   \
        case name ## _k: return to_json(#name);
        #include "SyntacticContext.cxx"
    }
}
