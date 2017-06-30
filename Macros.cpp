
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
                    PresumedLoc start = sm.getPresumedLoc(mi->getDefinitionLoc());
                    PresumedLoc end = sm.getPresumedLoc(mi->getDefinitionEndLoc());
                    Macro m(name.str(), body, start, end);

                    m_macros.insert(std::make_pair(m.hash(), m));
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
    for (auto const pair : m_macros) {
        const Macro m = pair.second;

        if (m.start().getFilename() != NULL &&
            m.end().getFilename() != NULL &&
            loc.getFilename() != NULL &&
            strcmp(m.start().getFilename(), loc.getFilename()) == 0 &&
            strcmp(m.end().getFilename(), loc.getFilename()) == 0 &&
            m.start().getLine() <= loc.getLine() &&
            m.start().getColumn() <= loc.getColumn() &&
            loc.getLine() <= m.end().getLine() &&
            loc.getColumn() <= m.end().getColumn()) {
            return MacroDB::find(m.hash());
        }
    }

    return NULL;
}
const Macro* MacroDB::find(const std::string & name) const {
    for (auto const pair : m_macros) {
        const Macro m = pair.second;
        if (m.name() == name) {
            return MacroDB::find(m.hash());
        }
    }

    return NULL;
}

const Macro* MacroDB::find(const Macro & macro) const {
    return find(macro.hash());
}

const Macro* MacroDB::find(const Hash & hash) const {
    return m_macros.find(hash) != m_macros.end() ?
           &m_macros.find(hash)->second :
           NULL;
}

picojson::array MacroDB::databaseToJSON() {
    picojson::array array;
    for (std::map<Hash, Macro>::iterator it = m_macros.begin();
         it != m_macros.end();
         ++it)
    {
        array.push_back(to_json(it->second));
    }
    return array;
}

} // end namespace clang_mutate
