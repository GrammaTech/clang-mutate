#ifndef GET_MACROS_H
#define GET_MACROS_H

#include "Hash.h"
#include "Json.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"

#include <string>
#include <stack>
#include <map>
#include <set>

namespace clang_mutate {

class Macro
{
public:
    Macro(const std::string & n,
          const std::string & b,
          const clang::PresumedLoc & start,
          const clang::PresumedLoc & end)
        : m_name(n)
        , m_body(b)
        , m_start(start)
        , m_end(end)
    {}

    bool operator<(const Macro & other) const
    {
        if (m_name != other.m_name)
            return m_name < other.m_name;
        return m_body < other.m_body;
    }

    Hash hash() const;

    const std::string name() const { return m_name; }
    const std::string body() const { return m_body; }
    const clang::PresumedLoc start() const { return m_start; }
    const clang::PresumedLoc end() const { return m_end; }
private:
    std::string m_name;
    std::string m_body;
    clang::PresumedLoc m_start;
    clang::PresumedLoc m_end;
};

class MacroDB
{
public:
    static MacroDB & getInstance(clang::CompilerInstance * _CI);

    MacroDB(const MacroDB &) = delete;
    MacroDB& operator=(const MacroDB &) = delete;

    const Macro* find(const clang::PresumedLoc & loc ) const;
    const Macro* find(const std::string & name) const;
    const Macro* find(const Macro & macro) const;
    const Macro* find(const Hash & hash) const;

    picojson::array databaseToJSON();
private:
    MacroDB() {}

    MacroDB(clang::CompilerInstance *CI);

    std::map<Hash, Macro> m_macros;
};

} // end namespace clang_mutate

template <> inline
picojson::value to_json(const clang_mutate::Macro & m)
{
    picojson::object jsonObj;

    jsonObj["hash"] = to_json(m.hash());
    jsonObj["name"] = to_json(m.name());
    jsonObj["body"] = to_json(m.body());

    return to_json(jsonObj);
}

#endif
