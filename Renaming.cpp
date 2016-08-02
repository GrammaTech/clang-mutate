
#include "Renaming.h"
#include "Utils.h"
#include "clang/Lex/Lexer.h"

#include <sstream>
#include <iostream>

using namespace clang;

RenameDatum mkVariableRename(
    clang::IdentifierInfo * id,
    const std::string & name,
    size_t index)
{ return RenameDatum(id, name, index, VariableRename); }

RenameDatum mkFunctionRename(
    clang::IdentifierInfo * id,
    const std::string & name)
{ return RenameDatum(id, name, 0, FunctionRename); }

RenameFreeVar::RenameFreeVar(
    Stmt * the_stmt,
    SourceManager & _sm,
    const LangOptions & _langOpts,
    SourceLocation _begin,
    SourceLocation _end,
    const Renames & rn)
    : sm(_sm)
    , langOpts(_langOpts)
    , begin(_begin)
    , end(_end)
    , renames(rn)
{
    TraverseStmt(the_stmt);
}

RenameFreeVar::RenameFreeVar(
    Decl * the_decl,
    SourceManager & _sm,
    const LangOptions & _langOpts,
    SourceLocation _begin,
    SourceLocation _end,
    const Renames & rn)
    : sm(_sm)
    , langOpts(_langOpts)
    , begin(_begin)
    , end(_end)
    , renames(rn)
{
    TraverseDecl(the_decl);
}

static bool find_identifier(const Renames & renames,
                            IdentifierInfo * id,
                            std::string & ans)
{
    std::ostringstream oss;
    for (std::set<VariableInfo>::const_iterator
             it = renames.first.begin(); it != renames.first.end(); ++it)
    {
        if (it->getId() == id) {
            oss << "(|" << it->getName() << "|)";
            ans = oss.str();
            return true;
        }
    }
    for (std::set<FunctionInfo>::const_iterator
             it = renames.second.begin(); it != renames.second.end(); ++it)
    {
        if (it->getId() == id) {
            oss << "(|" << it->getName() << "|)";
            ans = oss.str();
            return true;
        }
    }
    return false;
}

bool getRangeSize(const CharSourceRange & r,
                  const SourceManager & sm,
                  const LangOptions & langOpts,
                  int & result)
{
    SourceLocation sp_begin = sm.getSpellingLoc(r.getBegin());
    SourceLocation sp_end   = sm.getSpellingLoc(r.getEnd());
    std::pair<FileID, unsigned> begin = sm.getDecomposedLoc(sp_begin);
    std::pair<FileID, unsigned> end   = sm.getDecomposedLoc(sp_end);
    if (begin.first != end.first)
        return false;
    if (r.isTokenRange())
        end.second += Lexer::MeasureTokenLength(sp_end, sm, langOpts);
    result = end.second - begin.second;
    return true;
}

bool RenameFreeVar::VisitStmt(Stmt * stmt)
{
  if (isa<DeclRefExpr>(stmt)) {
    DeclRefExpr * declref = static_cast<DeclRefExpr*>(stmt);
    ValueDecl * vdecl     = declref->getDecl();
    IdentifierInfo * id   = vdecl->getIdentifier();
    std::string name;
    if (id != NULL && find_identifier(renames, id, name)) {
      SourceRange sr =
          Utils::getImmediateMacroArgCallerRange(
              sm,
              stmt->getSourceRange());
      if (sm.isInMainFile(sr.getBegin()) &&
          !sm.isMacroBodyExpansion(sr.getBegin()))
      {
        // Not very efficient, but I didn't see a better way to
        // get the size in characters of a CharSourceRange.
        CharSourceRange srcRange =
            CharSourceRange::getCharRange(begin, sr.getBegin());
        size_t offset = Lexer::getSourceText(
            srcRange,
            sm,
            langOpts,
            NULL).size();
        std::string old_str;
        llvm::raw_string_ostream ss(old_str);
        stmt->printPretty(ss, 0, PrintingPolicy(langOpts));
        old_str = ss.str();
        std::string new_str = name;

        int _;
        // If the statement is part of a macro expansion, getRangeSize will
        // return false; we want to skip the rewriting in this case.
        if (getRangeSize(srcRange, sm, langOpts, _)) {
            replacements.add(offset, old_str.size(), new_str);
        }
      }
    }
  }
  return true;
}

std::string RenameFreeVar::getRewrittenString() const
{
    std::string orig = Lexer::getSourceText(
        CharSourceRange::getCharRange(begin, end),
        sm,
        langOpts,
        NULL);
    return replacements.apply_to(orig);
}

std::set<std::string> RenameFreeVar::getIncludes() const
{ return includes; }

void Replacements::add(size_t offset,
                       size_t size,
                       const std::string & s)
{ replacements[offset] = std::make_pair(size, s); }

std::string Replacements::apply_to(const std::string & orig) const
{
    std::string ans;
    auto it = replacements.begin();
    for (size_t p = 0; p < orig.size(); ++p) {
        if (it != replacements.end() && p == it->first) {
            p += it->second.first - 1; // - 1 compensates ++p
            ans.append(it->second.second);
            ++it;
            continue;
        }
        ans.push_back(orig[p]);
    }
    return ans;
}
