#ifndef GET_MACROS_H
#define GET_MACROS_H

#include "Hash.h"
#include "Json.h"
#include "Utils.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"

#include <string>
#include <unordered_map>

namespace clang_mutate {

class Macro
{
public:
    Macro(const std::string & n,
          const std::string & b)
        : m_name(n)
        , m_body(b)
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
private:
    std::string m_name;
    std::string m_body;
};


class MacroDB
{
public:
    static MacroDB & getInstance(clang::CompilerInstance * _CI);

    MacroDB(const MacroDB &) = delete;
    MacroDB& operator=(const MacroDB &) = delete;

    const Macro* find(const clang::PresumedLoc & loc) const;

    picojson::array databaseToJSON();
private:
    struct PresumedLocHash
    {
        std::size_t operator()(const clang::PresumedLoc &loc) const;
    };

    struct PresumedLocEqualTo
    {
        bool operator()(const clang::PresumedLoc &lhs,
                        const clang::PresumedLoc &rhs) const;
    };

    typedef std::unordered_map<clang::PresumedLoc,
                               Macro,
                               PresumedLocHash,
                               PresumedLocEqualTo>
            MacroMap;

    MacroDB() {}
    MacroDB(clang::CompilerInstance *CI);

    MacroMap m_macros;
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
