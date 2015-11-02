
#include "Renaming.h"

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
        oss << "«" << next_id++ << "»";
        ans.insert(RenameDatum(*it, oss.str(), VariableRename));
    }
    for (it = free_funs.begin(); it != free_funs.end(); ++it) {
#ifdef ALLOW_FREE_FUNCTIONS
        std::ostringstream oss;
        oss << "«" << next_id++ << "»";
        ans.insert(RenameDatum(*it, oss.str(), FunctionRename));
#else
        ans.insert(RenameDatum(*it, (*it)->getName().str(), FunctionRename));
#endif
    }
    return ans;
}

static int stmt_offset_correction(Stmt * stmt)
{
    // This is really just guesswork based on what I've seen in
    // code snippets so far.
    return isa<Expr>(stmt) ? 1 : 2;
}

RenameFreeVar::RenameFreeVar(
    Stmt * the_stmt,
    Rewriter & r,
    const Renames & rn)
    : rewriter(r)
    , renames(rn)
{
    SourceManager & sm = rewriter.getSourceMgr();
    const SourceLocation & b = the_stmt->getSourceRange().getBegin();
    const SourceLocation & e = the_stmt->getSourceRange().getEnd();
    
    begin = sm.getCharacterData(b);

    // This is very awkward, but it seems to work as expected.
    // The problem is that some Stmts give an empty SourceRange,
    // so we can reliably figure out where they start but not
    // where they end (in particular, DeclRefStmt does this by
    // design, for reasons I really am not clear about).  So we
    // also require the caller to pass us the pretty-printed
    // length of the Stmt.  But *that* isn't always correct either,
    // since pretty printing will do its own formatting of things.
    // The hope is that there is nothing in the intersection of
    // empty-source-range Stmts with wrong-pretty-printed-length
    // Stmts.  This appears to at least be true for DeclRefStmt
    // nodes, as far as I can tell.
    //
    // Ugh.
    //
    end   = (b == e)
            ? begin + rewriter.ConvertToString(the_stmt).size()
        : sm.getCharacterData(e) + stmt_offset_correction(the_stmt);
    
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
      SourceManager & sm = rewriter.getSourceMgr();
      const char * ptr = sm.getCharacterData(sr.getBegin());
      std::string old_str = rewriter.ConvertToString(stmt);
      std::string new_str = name;
      rewrites[ptr] = std::make_pair(old_str.size(), new_str);
    }
  }
  return true;
}

std::string RenameFreeVar::getRewrittenString() const
{
    std::string ans;
    std::map<const char*, std::pair<size_t, std::string> >::const_iterator it;
    it = rewrites.begin();
    for (const char * p = begin; p < end; ++p) {
        if (it != rewrites.end() && p == it->first) {
            p += it->second.first - 1; // - 1 compensates ++p
            ans.append(it->second.second);
            ++it;
            continue;
        }
        ans.push_back(*p);
    }
    return ans;
}

