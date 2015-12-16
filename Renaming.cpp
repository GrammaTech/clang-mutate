
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

static std::string ident_to_str(IdentifierInfo* ident, size_t id)
{
    std::string name = ident->getName().str();
    std::ostringstream oss;
    oss << "(|";
    if (name != "")
        oss << name;
    else
        oss << id; // can this happen?
    oss << "|)";
    return oss.str();
}

Renames make_renames(
    const std::set<std::pair<IdentifierInfo*, size_t> > & free_vars,
    const std::set<IdentifierInfo*> & free_funs)
{
    size_t next_id = 0;
    Renames ans;
    for (std::set<std::pair<IdentifierInfo*,size_t> >::const_iterator
             it = free_vars.begin(); it != free_vars.end(); ++it)
    {
        ans.insert(mkVariableRename(it->first,
                                    ident_to_str(it->first, next_id++),
                                    it->second));
    }
    for (std::set<IdentifierInfo*>::const_iterator
             it = free_funs.begin(); it != free_funs.end(); ++it)
    {
#ifdef ALLOW_FREE_FUNCTIONS
        ans.insert(mkFunctionRename(*it, ident_to_str(*it, next_id++)));
#else
        ans.insert(mkFunctionRename(*it, (*it)->getName().str()));
#endif
    }
    return ans;
}

RenameFreeVar::RenameFreeVar(
    Stmt * the_stmt,
    Rewriter & r,
    const Renames & rn)
    : rewriter(r)
    , renames(rn)
{
    SourceRange sr = Utils::expandRange(rewriter.getSourceMgr(),
                                        rewriter.getLangOpts(),
                                        Utils::getImmediateMacroArgCallerRange(
                                            rewriter.getSourceMgr(),
                                            the_stmt->getSourceRange()));

    begin = sr.getBegin(); 
    end = Lexer::getLocForEndOfToken(sr.getEnd(), 
                                     0,
                                     rewriter.getSourceMgr(),
                                     rewriter.getLangOpts());
    TraverseStmt(the_stmt);
}

static bool find_identifier(const Renames & renames,
                            IdentifierInfo * id,
                            std::string & ans)
{
    for (Renames::const_iterator it = renames.begin();
         it != renames.end();
         ++it)
    {
        if (it->ident == id) {
            ans = it->name;
            return true;
        }
    }
    return false;
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
          Utils::expandRange(rewriter.getSourceMgr(),
                             rewriter.getLangOpts(),
                             Utils::getImmediateMacroArgCallerRange(
                                 rewriter.getSourceMgr(),
                                 stmt->getSourceRange()));

      // Not very efficient, but I didn't see a better way to
      // get the size in characters of a CharSourceRange.
      CharSourceRange srcRange = CharSourceRange::getCharRange(begin,
                                                               sr.getBegin());
      size_t offset = Lexer::getSourceText(
          srcRange,
          rewriter.getSourceMgr(),
          rewriter.getLangOpts(),
          NULL).size();
      std::string old_str;
      llvm::raw_string_ostream ss(old_str);
      stmt->printPretty(ss, 0, PrintingPolicy(rewriter.getLangOpts()));
      old_str = ss.str();
      std::string new_str = name;

      if (rewriter.getRangeSize(srcRange) != -1) {
          // The srcRange will have size -1 when the statement is
          // part of a macro expansion; in that case, we want to
          // skip the rewriting.
          rewrites[offset] = std::make_pair(old_str.size(), new_str);
      }
    }
  }
  return true;
}

std::string RenameFreeVar::getRewrittenString() const
{
    std::string orig = Lexer::getSourceText(
        CharSourceRange::getCharRange(begin, end),
        rewriter.getSourceMgr(),
        rewriter.getLangOpts(),
        NULL);
    std::string ans;
    RewriteMap::const_iterator it = rewrites.begin();
    for (size_t p = 0; p < orig.size(); ++p) {
        if (it != rewrites.end() && p == it->first) {
            p += it->second.first - 1; // - 1 compensates ++p
            ans.append(it->second.second);
            ++it;
            continue;
        }
        ans.push_back(orig[p]);
    }
    return ans;
}

