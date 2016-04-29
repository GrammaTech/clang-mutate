
#include "Macros.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

#include <sstream>

namespace clang_mutate {
using namespace clang;

bool GetMacros::VisitStmt(clang::Stmt * stmt)
{
    SourceManager & sm = CI->getSourceManager();
    LangOptions & langOpts = CI->getLangOpts();
    
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
            SourceLocation end =
                Lexer::getLocForEndOfToken(def.getEnd(), 0, sm, langOpts)
                .getLocWithOffset(1);

            for (SourceLocation it = def.getBegin();
                 it != end;
                 it = it.getLocWithOffset(1))
            {
                body.push_back(sm.getCharacterData(it)[0]);
            }
            while (body.back() == '\n' || body.back() == '\r')
                body.pop_back();

            std::string header;
            if (Utils::in_header(mi->getDefinitionLoc(), CI, header)) {
                includes.insert(header);
            }
            else {
                macros.insert(Macro(name.str(), body));
            }
        }
    }

    is_first = false;
    return true;
}

} // end namespace clang_mutate
