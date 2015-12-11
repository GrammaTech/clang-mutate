
#include "Macros.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

#include <sstream>

namespace clang_mutate {
using namespace clang;

bool GetMacros::VisitStmt(clang::Stmt * stmt)
{
    SourceRange R = stmt->getSourceRange();

    SourceLocation sb = sm.getSpellingLoc(R.getBegin());
    SourceLocation se = sm.getSpellingLoc(R.getEnd())
                                          .getLocWithOffset(1);
    SourceRange sr(sb, se);

    SourceLocation xb = sm.getExpansionRange(R.getBegin()).first;
    SourceLocation xe = sm.getExpansionRange(R.getEnd()).second;
    SourceRange xr(xb, xe);

    std::pair<SourceLocation,SourceLocation> ixbe =
        sm.getImmediateExpansionRange(R.getBegin());
    SourceRange ixr(ixbe.first, ixbe.second);
    
    if (xr == ixr) {
        if (is_first == true)
            toplev_is_macro = true;
        Preprocessor & pp = CI->getPreprocessor();
        StringRef name = pp.getImmediateMacroName(stmt->getLocStart());
        MacroInfo * mi = pp.getMacroInfo(pp.getIdentifierInfo(name));

        if ( mi != NULL ){
            SourceRange def(mi->getDefinitionLoc(), mi->getDefinitionEndLoc());
            
            std::string body;
            SourceLocation end = def.getEnd().getLocWithOffset(2);
            for (SourceLocation it = def.getBegin();
                 it != end;
                 it = it.getLocWithOffset(1))
            {
                body.push_back(sm.getCharacterData(it)[0]);
            }
            while (body.back() == '\n' || body.back() == '\r')
                body.pop_back();
            macros.insert(Macro(name.str(), body));
        }
    }

    is_first = false;
    return true;
}

} // end namespace clang_mutate
