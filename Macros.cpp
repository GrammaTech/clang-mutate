
#include "Macros.h"
#include "Utils.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

#include <functional>
#include <sstream>
#include <utility>

namespace clang_mutate {
using namespace clang;

std::string macro_body(clang::CompilerInstance *ci,
                       clang::MacroInfo *mi)
{
    SourceRange def(mi->getDefinitionLoc(), mi->getDefinitionEndLoc());
    LangOptions & langOpts = ci->getLangOpts();
    SourceManager & sm = ci->getSourceManager();
    SourceLocation end =
        Lexer::getLocForEndOfToken(def.getEnd(), 0, sm, langOpts)
        .getLocWithOffset(1);
    std::string body;

    for (SourceLocation it = def.getBegin();
         it != end;
         it = it.getLocWithOffset(1))
    {
        body.push_back(sm.getCharacterData(it)[0]);
    }
    while (body.back() == '\n' || body.back() == '\r')
        body.pop_back();

    return body;
}

Hash Macro::hash() const {
    std::hash<std::string> hasher;
    return Hash(hasher(m_name + m_body));
}

std::size_t MacroDB::PresumedLocHash::operator()(const PresumedLoc & loc) const
{
    std::stringstream ss;

    if (loc.isInvalid()) {
        ss << "<invalid>";
    }
    else {
        ss << loc.getFilename() << ":"
           << loc.getLine() << ":"
           << loc.getColumn();
    }

    return std::hash<std::string>()(ss.str());
}

bool MacroDB::PresumedLocEqualTo::operator() (const PresumedLoc & lhs,
                                              const PresumedLoc & rhs) const
{
    return ((lhs.isInvalid() && rhs.isInvalid()) ||
            (!lhs.isInvalid() && !rhs.isInvalid() &&
             strcmp(lhs.getFilename() ? lhs.getFilename() : "",
                    rhs.getFilename() ? rhs.getFilename() : "") == 0 &&
             lhs.getLine() == rhs.getLine() &&
             lhs.getColumn() == rhs.getColumn()));
}

MacroDB::MacroDB(clang::CompilerInstance * _CI) {
    if (_CI) {
        Preprocessor & pp = _CI->getPreprocessor();
        SourceManager & sm = _CI->getSourceManager();

        for (auto it = pp.getIdentifierTable().begin();
             it != pp.getIdentifierTable().end();
             it++) {
            MacroInfo * mi = pp.getMacroInfo(it->getValue());

            if (mi != NULL) {
                StringRef name = it->getValue()->getName();
                std::string body = macro_body(_CI, mi);

                if (sm.isWrittenInMainFile(mi->getDefinitionLoc())) {
                    Macro m(name.str(), body);
                    m_macros.insert(
                        std::make_pair(
                            sm.getPresumedLoc(mi->getDefinitionLoc()),
                            m));
                }
            }
        }
    }
}

MacroDB & MacroDB::getInstance(CompilerInstance * _CI) {
    static MacroDB macroDB(_CI);
    return macroDB;
}

const Macro* MacroDB::find(const PresumedLoc & loc) const {
    return m_macros.find(loc) != m_macros.end() ?
           &m_macros.find(loc)->second :
           NULL;
}

picojson::array MacroDB::databaseToJSON() {
    picojson::array array;
    for (MacroMap::iterator it = m_macros.begin();
         it != m_macros.end();
         ++it)
    {
        array.push_back(to_json(it->second));
    }
    return array;
}

} // end namespace clang_mutate
