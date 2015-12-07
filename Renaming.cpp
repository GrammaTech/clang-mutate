
#include "Renaming.h"
#include "clang/Lex/Lexer.h"

#include <sstream>
#include <iostream>

using namespace clang;

Renames make_renames(const std::set<IdentifierInfo*> & free_vars,
                     const std::set<IdentifierInfo*> & free_funs)
{
    size_t next_id = 0;
    Renames ans;
    std::set<IdentifierInfo*>::const_iterator it;
    for (it = free_vars.begin(); it != free_vars.end(); ++it) {
        std::ostringstream oss;
        oss << "(|" << next_id++ << "|)";
        ans.insert(RenameDatum(*it, oss.str(), VariableRename));
    }
    for (it = free_funs.begin(); it != free_funs.end(); ++it) {
#ifdef ALLOW_FREE_FUNCTIONS
        std::ostringstream oss;
        oss << "(|" << next_id++ << "|)";
        ans.insert(RenameDatum(*it, oss.str(), FunctionRename));
#else
        ans.insert(RenameDatum(*it, (*it)->getName().str(), FunctionRename));
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
    begin = the_stmt->getSourceRange().getBegin();
    end = Lexer::getLocForEndOfToken(the_stmt->getSourceRange().getEnd(), 0,
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
      SourceRange sr = stmt->getSourceRange();

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

