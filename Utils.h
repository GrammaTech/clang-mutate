
#ifndef CLANG_MUTATE_UTILS_H
#define CLANG_MUTATE_UTILS_H

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
}

#endif
