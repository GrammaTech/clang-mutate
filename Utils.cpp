#include "Utils.h"
#include "Ast.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace clang_mutate;
using namespace clang;

namespace Utils {

std::string safe_realpath(const std::string & filename)
{
    char * buf = realpath(filename.c_str(), NULL);
    if (buf != NULL) {
        std::string ans(buf);
        free(buf);
        return ans;
    }
    return "";
}

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
    return SM.isInMainFile(loc);
}

SourceRange normalizeSourceRange(SourceRange r,
                                 bool expand_to_semicolon,
                                 SourceManager & sm,
                                 const LangOptions & langOpts)
{
    return getImmediateMacroArgCallerRange(
        sm,
        expand_to_semicolon ? expandRange(sm, langOpts, r) : r);
}


bool ShouldVisitStmt(SourceManager & SM,
                     const LangOptions & LangOpts,
                     FileID mainFileID,
                     clang::Stmt * stmt)
{
    SourceRange r;

    if (stmt == NULL || stmt->getStmtClass() == Stmt::NoStmtClass) {
        return false;
    }
    else {    
        r = expandRange(SM, LangOpts, stmt->getSourceRange());
        return SelectRange(SM, mainFileID, r);
    }
}

bool ShouldVisitDecl(SourceManager & SM,
                     const LangOptions & LangOpts,
                     FileID mainFileID,
                     clang::Decl * decl)
{
    SourceRange r;

    if (decl == NULL) {
        return false;
    }
    else {    
        r = expandRange(SM, LangOpts, decl->getSourceRange());
        return SelectRange(SM, mainFileID, r);
    }
}

bool ShouldAssociateCompilationDataWithStmt(Stmt *S, Stmt *P)
{
    if ( S != NULL )
    {
        if (S->getStmtClass() == Stmt::BreakStmtClass ||
            S->getStmtClass() == Stmt::CapturedStmtClass ||
            S->getStmtClass() == Stmt::CompoundStmtClass ||
            S->getStmtClass() == Stmt::ContinueStmtClass ||
            S->getStmtClass() == Stmt::CXXCatchStmtClass ||
            S->getStmtClass() == Stmt::CXXForRangeStmtClass ||
            S->getStmtClass() == Stmt::CXXTryStmtClass ||
            S->getStmtClass() == Stmt::DeclStmtClass ||
            S->getStmtClass() == Stmt::DoStmtClass ||
            S->getStmtClass() == Stmt::ForStmtClass ||
            S->getStmtClass() == Stmt::GotoStmtClass ||
            S->getStmtClass() == Stmt::IfStmtClass ||
            S->getStmtClass() == Stmt::IndirectGotoStmtClass ||
            S->getStmtClass() == Stmt::ReturnStmtClass ||
            S->getStmtClass() == Stmt::SwitchStmtClass ||
            S->getStmtClass() == Stmt::DefaultStmtClass ||
            S->getStmtClass() == Stmt::CaseStmtClass ||
            S->getStmtClass() == Stmt::WhileStmtClass ||
            IsSingleLineStmt(S, P)) {
            return true;
        }
    }

    return false;
}

// Return true if the clang::Stmt is a statement in the C++ grammar
// which would typically be a single line in a program.
// This is done by testing if the parent of the clang::Stmt
// is an aggregation type.  The immediate children of an aggregation
// type are all valid statements in the C/C++ grammar.
bool IsSingleLineStmt(Stmt *S, Stmt *P)
{
    if (S != NULL && P != NULL) {
        switch (P->getStmtClass()){
        case Stmt::CompoundStmtClass:
        {
            return true;
        }
        case Stmt::CapturedStmtClass:
        {
            CapturedStmt * cs = static_cast<CapturedStmt*>(P);
            return cs->getCapturedStmt() == S;
        }
        case Stmt::CXXCatchStmtClass:
        {
            CXXCatchStmt * cs = static_cast<CXXCatchStmt*>(P);
            return cs->getHandlerBlock() == S;
        }
        case Stmt::CXXForRangeStmtClass:
        {
            CXXForRangeStmt * fs = static_cast<CXXForRangeStmt*>(P);
            return fs->getBody() == S;
        }
        case Stmt::DoStmtClass:
        {
            DoStmt *ds = static_cast<DoStmt*>(P);
            return ds->getBody() == S;
        }
        case Stmt::ForStmtClass:
        {
            ForStmt *fs = static_cast<ForStmt*>(P);
            return fs->getBody() == S;
        }
        case Stmt::IfStmtClass:
        {
            IfStmt *is = static_cast<IfStmt*>(P);
            return is->getThen() == S || is->getElse() == S;
        }
        case Stmt::SwitchStmtClass:
        {
            SwitchStmt *ss = static_cast<SwitchStmt*>(P); 
            return ss->getBody() == S;
        }
        case Stmt::WhileStmtClass:
        {
            WhileStmt *ws = static_cast<WhileStmt*>(P);
            return ws->getBody() == S;
        }
        default:
            return false;
        }
    }

    return false;
}

// Return true if S is a top-level statement within a 
// loop/if conditional.
bool IsGuardStmt(Stmt *S, Stmt *P)
{
    if (!IsSingleLineStmt(S, P) && P != NULL &&
        (P->getStmtClass() == Stmt::CapturedStmtClass ||
         P->getStmtClass() == Stmt::CompoundStmtClass || 
         P->getStmtClass() == Stmt::CXXCatchStmtClass ||
         P->getStmtClass() == Stmt::CXXForRangeStmtClass ||
         P->getStmtClass() == Stmt::CXXTryStmtClass ||
         P->getStmtClass() == Stmt::DoStmtClass ||
         P->getStmtClass() == Stmt::ForStmtClass ||
         P->getStmtClass() == Stmt::IfStmtClass ||
         P->getStmtClass() == Stmt::SwitchStmtClass ||
         P->getStmtClass() == Stmt::WhileStmtClass)) {
         return true;
    }

    return false;
}

// Return true if statement is the body of a loop or if/else statement.
bool IsLoopOrIfBody(Stmt *S, Stmt *P)
{
    return (P != NULL && IsSingleLineStmt(S, P) &&
            (P->getStmtClass() == Stmt::IfStmtClass ||
             P->getStmtClass() == Stmt::WhileStmtClass ||
             P->getStmtClass() == Stmt::DoStmtClass ||
             P->getStmtClass() == Stmt::ForStmtClass));
}

bool fileExists(const std::string & path)
{
    std::ifstream infile(path);
    return infile.good();
}

std::string filenameToContents(const std::string & str)
{
    std::stringstream buffer;

    std::ifstream f(str.c_str());
    if (f.good()) {
        buffer << f.rdbuf();
    }

    return buffer.str();
}

std::string trim(const std::string &input)
{
    std::string::const_iterator it = input.begin();
    while (it != input.end() && isspace(*it))
        it++;

    std::string::const_reverse_iterator rit = input.rbegin();
    while (rit.base() != it && isspace(*rit))
        rit++;

    return std::string(it, rit.base());
}

std::vector<std::string> split(const std::string &input,
                               const char delim)
{
    std::vector<std::string> elems;
    std::stringstream ss(input);
    std::string tok;

    while (getline(ss, tok, delim)) {
        elems.push_back(tok);
    }

    return elems;
}

std::string intercalate(const std::vector<std::string> & input,
                        const std::string & between)
{
    if (input.empty())
        return "";

    std::ostringstream oss;
    bool first = true;
    for (auto & text : input) {
        oss << (first ? "" : between) << text;
        first = false;
    }
    return oss.str();
}

bool in_header(
    SourceLocation loc,
    CompilerInstance * ci,
    std::string & header)
{
    SourceManager & sm = ci->getSourceManager();
    if (sm.isWrittenInMainFile(loc))
        return false;

    SourceLocation last_hdr = loc;
    if (sm.isInSystemHeader(loc) || sm.isInExternCSystemHeader(loc)) {
        // loc is in a system header
        while (sm.isInSystemHeader(loc) || sm.isInExternCSystemHeader(loc)) {
            last_hdr = loc;
            loc = sm.getIncludeLoc(sm.getFileID(loc));
        }

        last_hdr = sm.getSpellingLoc(last_hdr);
        PresumedLoc pl = sm.getPresumedLoc(last_hdr);
        std::string filename = Utils::safe_realpath(pl.getFilename());
        header = "";

        for (auto it = ci->getHeaderSearchOpts().UserEntries.begin();
             it != ci->getHeaderSearchOpts().UserEntries.end() &&
               header.empty();
             it++) {
            std::string path = Utils::safe_realpath(it->Path);
            if (!path.empty() && filename.find(path) != std::string::npos) {
                header = "<" + filename.substr(path.length() + 1) + ">";
            }
        }

        if (header.empty())
            return false;
    }
    else {
        if (!loc.isValid())
            return false;

        PresumedLoc pl = sm.getPresumedLoc(sm.getSpellingLoc(loc));
        std::string filename = Utils::safe_realpath(pl.getFilename());
        header = "";

        for (auto it = ci->getHeaderSearchOpts().UserEntries.begin();
             it != ci->getHeaderSearchOpts().UserEntries.end() &&
               header.empty();
             it++) {
            std::string path = Utils::safe_realpath(it->Path);
            if (!path.empty() && filename.find(path) != std::string::npos) {
                header = "" + filename.substr(path.length() + 1) + "";
            }
        }

        if (header.empty()) {
            if (filename.find_last_of("/") != std::string::npos)
                header = filename.substr(filename.find_last_of("/") + 1);
            else
                return false;
        }
    }

    return true;
}

std::string ltrim(const std::string & s, const std::string & t){
    std::string trimmed(s);
    return trimmed.substr(trimmed.find_first_not_of(t));
}

std::string rtrim(const std::string & s, const std::string & t){
    std::string trimmed(s);
    return trimmed.substr(0, trimmed.find_last_not_of(t) + 1);
}

std::string trim(const std::string & s, const std::string & t){
    return ltrim(rtrim(s, t), t);
}
std::string escape(const std::string & s)
{
    std::string ans("\"");
    for (size_t i = 0; i < s.size(); ++i) {
        switch (s[i]) {
        case '\t': ans.append("\\t");  continue;
        case '\n': ans.append("\\n");  continue;
        case '\r': ans.append("\\r");  continue;
        case '\\': ans.append("\\\\"); continue;
        case '\"': ans.append("\\\""); continue;
        case '\'': ans.append("\\\'"); continue;
        default:   ans.push_back(s[i]);
        }
    }
    ans.push_back('\"');
    return ans;
}

std::string unescape(const std::string & s)
{
    size_t last = s.size() - 1;
    if (s.empty() || s[0] != '\"' || s[last] != '\"')
        return s;
    std::string ans;
    bool in_escape = false;
    for (size_t i = 1; i < last; ++i) {
        if (in_escape) {
            char c;
            switch (s[i]) {
            case  't': c = '\t'; break;
            case  'n': c = '\n'; break;
            case  'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\"': c = '\"'; break;
            case '\'': c = '\''; break;
            default:   c = s[i]; break;
            }
            ans.push_back(c);
            in_escape = false;
            continue;
        }
        if (s[i] == '\\') {
            in_escape = true;
            continue;
        }
        ans.push_back(s[i]);
    }
    if (in_escape)
        ans.push_back('\\');
    return ans;
}

std::vector<std::string> tokenize(const std::string & s)
{
    std::vector<std::string> ans;
    char c;
    std::istringstream iss(s, std::istringstream::in);
    bool in_word = false;
    std::string word;
    bool escaped = false;
    bool in_quotes = false;
    size_t i;
    for (i = 0; i < s.size(); ++i) {
        c = s[i];
        if (in_quotes) {
            if (c == '\"' && !escaped) {
                word.push_back(c);
                ans.push_back(unescape(word));
                word = "";
                in_quotes = false;
                escaped = false;
                in_word = false;
                continue;
            }
            if (escaped) {
                escaped = false;
            }
            else if (c == '\\') {
                escaped = true;
            }
            word.push_back(c);
            continue;
        }
        if (isspace(c)) {
            if (!in_word)
                continue;
            in_word = false;
            ans.push_back(word);
            word = "";
            continue;
        }
        if (!in_word) {
            in_word = true;
            escaped = false;
            in_quotes = (c == '\"');
            word = "";
        }
        word.push_back(c);
    }
    if (in_word)
        ans.push_back(word);
    return ans;
}

bool read_uint(const std::string & s, unsigned int & n)
{
    std::istringstream iss(s, std::istringstream::in);
    return (bool)(iss >> n);
}

bool read_yesno(const std::string & s, bool & yesno)
{
    std::istringstream iss(s, std::istringstream::in);
    std::string yn;
    iss >> yn;
    if (yn == "yes") {
        yesno = true;
        return true;
    }
    if (yn == "no") {
        yesno = false;
        return true;
    }
    return false;
}

bool is_function_decl(AstRef ast, CompilerInstance const& ci)
{
    return ast->isDecl() && isa<FunctionDecl>(ast->asDecl(ci));
}

bool is_full_stmt(Stmt * stmt, AstRef parent, CompilerInstance const& ci)
{
    std::string parentClass = parent->className();
    std::string stmtClass = std::string(stmt->getStmtClassName());

    return (parentClass == "CompoundStmt" && stmtClass != "CompoundStmt")
        || parentClass == "LabelStmt"
        || (is_function_decl(parent, ci) && stmtClass == "CompoundStmt")
        // The first child of a CaseStmt is the condition. The rest are full
        // statements.
        || (parentClass == "CaseStmt" &&
            // If stmt is first child, it may not have been added to parent yet
            parent->children().size() > 0 &&
            stmt != parent->children()[0]->asStmt(ci))
        || (parentClass == "DoStmt" && stmtClass != "CompoundStmt" &&
            stmt == static_cast<DoStmt*>(parent->asStmt(ci))->getBody())
        || (parentClass == "ForStmt" && stmtClass != "CompoundStmt" &&
            stmt == static_cast<ForStmt*>(parent->asStmt(ci))->getBody())
        || (parentClass == "CXXForRangeStmt" && stmtClass != "CompoundStmt" &&
            stmt == static_cast<CXXForRangeStmt*>(parent->asStmt(ci))->getBody())
        || (parentClass == "WhileStmt" && stmtClass != "CompoundStmt" &&
            stmt == static_cast<WhileStmt*>(parent->asStmt(ci))->getBody())
        || (parentClass == "IfStmt" && stmtClass != "CompoundStmt" &&
            (stmt == static_cast<IfStmt*>(parent->asStmt(ci))->getThen() ||
             stmt == static_cast<IfStmt*>(parent->asStmt(ci))->getElse()));
}

bool is_full_stmt(Decl * decl, AstRef parent, CompilerInstance const& ci)
{
    return parent == NoAst;
}

} // end namespace Utils

picojson::value Hash::toJSON() const
{
    return to_json(static_cast<int64_t>(m_hash));
}
