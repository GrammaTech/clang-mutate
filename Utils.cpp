#include "Utils.h"

using namespace clang;

namespace Utils {

// This function adapted from clang/lib/ARCMigrate/Transforms.cpp
SourceLocation
findSemiAfterLocation(SourceManager & SM,
                      const LangOptions & LangOpts,
                      SourceLocation loc,
                      int Offset)
{
    if (loc.isMacroID()) {
        if (!Lexer::isAtEndOfMacroExpansion(loc, SM, LangOpts, &loc))
            return SourceLocation();
    }
    loc = Lexer::getLocForEndOfToken(loc, Offset, SM, LangOpts);
    
    // Break down the source location.
    std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(loc);
    
    // Try to load the file buffer.
    bool invalidTemp = false;
    StringRef file = SM.getBufferData(locInfo.first, &invalidTemp);
    if (invalidTemp)
        return SourceLocation();
    
    const char *tokenBegin = file.data() + locInfo.second;
    
    // Lex from the start of the given location.
    Lexer lexer(SM.getLocForStartOfFile(locInfo.first),
                LangOpts,
                file.begin(), tokenBegin, file.end());
    Token tok;
    lexer.LexFromRawLexer(tok);
    if (tok.isNot(tok::semi))
        return SourceLocation();
    return tok.getLocation();
}

SourceRange
expandRange(SourceManager & SM,
            const LangOptions & LangOpts,
            SourceRange r)
{
    // If the range is a full statement, and is followed by a
    // semi-colon then expand the range to include the semicolon.
    SourceLocation b = r.getBegin();
    SourceLocation e = findSemiAfterLocation(SM, LangOpts, r.getEnd());
    if (e.isInvalid()) e = r.getEnd();
    return SourceRange(b,e);
}

SourceRange expandSpellingLocationRange(SourceManager & SM,
                                        const clang::LangOptions & LangOpts,
                                        clang::SourceRange r)
{
    return expandRange(SM,
                       LangOpts,
                       getSpellingLocationRange(SM, r));
}

SourceRange getSpellingLocationRange(SourceManager & sm,
                                     SourceRange r)
{
    SourceLocation sb = sm.getSpellingLoc(r.getBegin());
    SourceLocation se = sm.getSpellingLoc(r.getEnd());
    return SourceRange(sb, se);
}

SourceRange getImmediateMacroArgCallerRange(SourceManager & sm,
                                            SourceRange r)
{
    SourceLocation b = r.getBegin();
    SourceLocation e = r.getEnd();

    if (b.isMacroID() && e.isMacroID() &&
        sm.isMacroArgExpansion(b) && sm.isMacroArgExpansion(e))
    {
        b = sm.getImmediateMacroCallerLoc(r.getBegin());
        e = sm.getImmediateMacroCallerLoc(r.getEnd());
    }

    return SourceRange(b, e);
}

bool SelectRange(SourceManager & SM,
                 FileID mainFileID,
                 SourceRange r)
{
    FullSourceLoc loc = FullSourceLoc(r.getEnd(), SM);
    return SM.isInMainFile(loc) && 
           !SM.isMacroBodyExpansion(loc);
}


bool ShouldVisitStmt(SourceManager & SM,
                     const LangOptions & LangOpts,
                     FileID mainFileID,
                     clang::Stmt * stmt)
{
    SourceRange r;

    if (stmt->getStmtClass() == Stmt::NoStmtClass) {
        return false;
    }
    else {    
        r = expandRange(SM, LangOpts, stmt->getSourceRange());
        return SelectRange(SM, mainFileID, r);
    }
}

// Return true if the clang::Expr is a statement in the C/C++ grammar.
// This is done by testing if the parent of the clang::Expr
// is an aggregation type.  The immediate children of an aggregation
// type are all valid statements in the C/C++ grammar.
bool IsCompleteCStmt(Stmt *S, Stmt *P)
{

    if ( P != NULL ) 
    {
        // Test if the parent is class containing
        // sub-statements.
        switch ( P->getStmtClass() )
        {
        case Stmt::CapturedStmtClass:
        case Stmt::CompoundStmtClass:
        case Stmt::CXXCatchStmtClass:
        case Stmt::CXXForRangeStmtClass:
        case Stmt::CXXTryStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::ForStmtClass:
        case Stmt::IfStmtClass:
        case Stmt::SwitchStmtClass:
        case Stmt::WhileStmtClass: 
          return true;
      
        default:
          return false;
        }
    }
    else {
        // Return true if this is the root of an AST, false otherwise.
        return (P == NULL) && 
               (S != NULL) &&
               (S->getStmtClass() == Stmt::CompoundStmtClass);
    }
}

}
