
#ifndef CLANG_MUTATE_UTILS_H
#define CLANG_MUTATE_UTILS_H

#include "Json.h"

#include "clang/AST/AST.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

namespace Utils {

clang::SourceLocation
findSemiAfterLocation(clang::SourceManager & SM,
                      const clang::LangOptions & LangOpts,
                      clang::SourceLocation loc,
                      int Offset = 0);

clang::SourceRange
expandRange(clang::SourceManager & SM,
            const clang::LangOptions & LangOpts,
            clang::SourceRange r);

clang::SourceRange
expandSpellingLocationRange(clang::SourceManager & SM,
                            const clang::LangOptions & LangOpts,
                            clang::SourceRange r);

clang::SourceRange getSpellingLocationRange(clang::SourceManager &SM,
                                            clang::SourceRange r);

clang::SourceRange
getImmediateMacroArgCallerRange(clang::SourceManager & SM,
                                clang::SourceRange r);


bool SelectRange(clang::SourceManager & SM,
                 clang::FileID mainFileID,
                 clang::SourceRange r);

bool ShouldVisitStmt(clang::SourceManager & SM,
                     const clang::LangOptions & LangOpts,
                     clang::FileID MainFileID,
                     clang::Stmt * stmt);

bool ShouldAssociateBytesWithStmt(clang::Stmt *S, clang::Stmt *P);
bool IsSingleLineStmt(clang::Stmt *S, clang::Stmt *P);
bool IsGuardStmt(clang::Stmt *S, clang::Stmt *P);

std::string filenameToContents(const std::string & str);

std::string trim(const std::string &input);
std::vector<std::string> split(const std::string &input,
                               const char delim);

bool is_system_header(clang::SourceLocation loc,
                      clang::SourceManager & sm,
                      std::string & header);

} // end namespace Utils

class Hash
{
public:
    Hash() : m_hash(0) {}
    Hash(size_t hash) : m_hash(hash) {}
    Hash(const Hash & h) : m_hash(h.m_hash) {}

    bool operator<(const Hash & h) const
    { return m_hash < h.m_hash; }

    bool operator<=(const Hash & h) const
    { return m_hash <= h.m_hash; }

    bool operator>(const Hash & h) const
    { return m_hash > h.m_hash; }

    bool operator>=(const Hash & h) const
    { return m_hash >= h.m_hash; }

    bool operator==(const Hash & h) const
    { return m_hash == h.m_hash; }

    bool operator!=(const Hash & h) const
    { return m_hash != h.m_hash; }

    picojson::value toJSON() const;

    size_t hash() const { return m_hash; }

private:
    size_t m_hash;
};

template <> inline
picojson::value to_json(const Hash & h)
{ return h.toJSON(); }

template <typename K, typename V>
std::vector<V> map_values(const std::map<K,V> & m)
{
    std::vector<V> ans;
    for (typename std::map<K,V>::const_iterator
             it = m.begin(); it != m.end(); ++it)
    {
        ans.push_back(it->second);
    }
    return ans;
}

#endif
