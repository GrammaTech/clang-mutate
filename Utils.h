
#ifndef CLANG_MUTATE_UTILS_H
#define CLANG_MUTATE_UTILS_H

#include "Json.h"
#include "Hash.h"

#include "clang/AST/AST.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

template <typename T>
struct Reversed
{
    typedef typename T::const_reverse_iterator const_iterator;
    const_iterator begin() const { return x.rbegin(); }
    const_iterator end()   const { return x.rend(); }
    Reversed(const T & _x) : x(_x) {}
private:
    const T & x;
};

template <typename T> Reversed<T> reversed(const T & x)
{ return Reversed<T>(x); }

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

bool ShouldVisitDecl(clang::SourceManager & SM,
                     const clang::LangOptions & LangOpts,
                     clang::FileID MainFileID,
                     clang::Decl * decl);

bool ShouldAssociateBytesWithStmt(clang::Stmt *S, clang::Stmt *P);
bool IsSingleLineStmt(clang::Stmt *S, clang::Stmt *P);
bool IsGuardStmt(clang::Stmt *S, clang::Stmt *P);

std::string filenameToContents(const std::string & str);

std::string trim(const std::string &input);
std::vector<std::string> split(const std::string &input,
                               const char delim);

bool in_system_header(clang::SourceLocation loc,
                      clang::SourceManager & sm,
                      std::string & header);

template <typename T>
std::set<T> set_union(const std::set<T> & xs,
                      const std::set<T> & ys)
{
    std::set<T> ans = ys;
    for (typename std::set<T>::const_iterator
             it = xs.begin(); it != xs.end(); ++it)
    {
        if (ys.find(*it) == ys.end())
            ans.insert(*it);
    }
    return ans;
}

std::vector<std::string> split(const std::string & s);

bool read_uint(const std::string & s, unsigned int & n);

bool read_yesno(const std::string & s, bool & yesno);

} // end namespace Utils

#endif
