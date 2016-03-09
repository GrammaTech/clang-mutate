#ifndef CLANG_MUTATE_AST_H
#define CLANG_MUTATE_AST_H

#include <vector>
#include <string>

#include "clang/AST/AST.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>

namespace clang_mutate {

typedef size_t AstRef;
const AstRef NoAst = 0;

class Ast
{
public:
    clang::Stmt * getClangStmt() const
    { return m_stmt; }

    clang::SourceRange sourceRange() const
    { return m_range; }
    
    std::string className() const
    { return m_class; }
    
    AstRef parent() const
    { return m_parent; }

    std::vector<AstRef> children() const
    { return m_children; }
    
    typedef std::vector<AstRef>::const_iterator child_iterator;

    child_iterator begin_children() const
    { return m_children.begin(); }

    child_iterator end_children() const
    { return m_children.end(); }

    Ast(clang::Stmt * _stmt, AstRef _parent)
        : m_stmt(_stmt)
        , m_parent(_parent)
        , m_children()
        , m_class(std::string(_stmt->getStmtClassName()))
        , m_range(_stmt->getSourceRange())
    {}

    void add_child(AstRef child)
    { m_children.push_back(child); }
    
private:
    clang::Stmt * m_stmt;
    AstRef m_parent;
    std::vector<AstRef> m_children;

    // These are all traits of the Stmt that do not seem
    // to be recoverable after the ActionFactory has run.
    // We cache them here; if we can figure out how to
    // get clang to keep the appropriate structures in
    // memory, then we can avoid the caching entirely.
    //
    // To catch things like this, load two or more files
    // into clang-mutate at once and try to process ASTs
    // in the first file. For the last file processed,
    // the relevant clang data structures stay alive,
    // making it harder to notice bugs where we try to
    // access vanished data.
    //
    std::string m_class;
    clang::SourceRange m_range;
};

class AstTable
{
public:
    AstRef create(clang::Stmt * stmt, AstRef parent)
    {
        asts.push_back(Ast(stmt, parent));
        return asts.size();
    }

    Ast& ast(AstRef ref)
    {
        assert (ref != 0 && ref <= count());
        return asts[ref - 1];
    }

    Ast& operator[](AstRef ref) { return ast(ref); }

    size_t count()
    { return asts.size(); }
    
private:
    std::vector<Ast> asts;
};

extern std::vector<std::pair<clang::CompilerInstance*, AstTable> > TUs;

} // namespace clang_mutate

#endif
