
#ifndef CLANG_MUTATE_UTILS_H
#define CLANG_MUTATE_UTILS_H

#include "AstRef.h"
#include "Json.h"
#include "Hash.h"

#include "clang/AST/AST.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/Frontend/CompilerInstance.h"

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

std::string escape  (const std::string & s);
std::string unescape(const std::string & s);
std::string ltrim   (const std::string & s, const std::string & t);
std::string rtrim   (const std::string & s, const std::string & t);
std::string trim    (const std::string & s, const std::string & t);

std::string safe_realpath(const std::string & filename);

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

clang::SourceRange
normalizeSourceRange(clang::SourceRange r,
                     bool expand_to_semicolon,
                     clang::SourceManager & sm,
                     const clang::LangOptions & langOpts);

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
bool IsLoopOrIfBody(clang::Stmt *stmt, clang::Stmt *parent);

std::string filenameToContents(const std::string & str);

std::string trim(const std::string &input);
std::vector<std::string> split(const std::string &input,
                               const char delim);
std::string intercalate(const std::vector<std::string> & input,
                        const std::string & between);

bool in_header(clang::SourceLocation loc,
               clang::CompilerInstance * ci,
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

std::vector<std::string> tokenize(const std::string & s);

bool read_uint(const std::string & s, unsigned int & n);

bool read_yesno(const std::string & s, bool & yesno);

bool is_function_decl(clang_mutate::AstRef ast,
                      clang::CompilerInstance const& ci);

template <typename T>
bool is_full_stmt(T * clang_obj, clang_mutate::AstRef parent,
                  clang::CompilerInstance const&);
bool is_full_stmt(clang::Stmt * stmt, clang_mutate::AstRef parent,
                  clang::CompilerInstance const&);
bool is_full_stmt(clang::Decl * decl, clang_mutate::AstRef parent,
                  clang::CompilerInstance const&);

} // end namespace Utils

#endif
