#ifndef CLANG_MUTATE_AST_H
#define CLANG_MUTATE_AST_H

#include <vector>
#include <string>

#include "Hash.h"
#include "PointedTree.h"
#include "Macros.h"
#include "Renaming.h"

#include "clang/AST/AST.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>

namespace clang_mutate {

typedef size_t AstRef;
const AstRef NoAst = 0;

class Ast
{
public:
    bool isDecl() const { return (m_decl != NULL); }
    clang::Decl * asDecl() const { return m_decl; }
    
    bool isStmt() const { return (m_stmt != NULL); }
    clang::Stmt * asStmt() const { return m_stmt; }
    
    clang::SourceRange sourceRange() const
    { return m_range; }

    std::string className() const
    { return m_class; }

    bool isGuard() const
    { return m_guard; }

    void setIsGuard(bool guard)
    { m_guard = guard; }

    void setIncludes(const std::set<std::string> & includes)
    { m_includes = includes; }

    void setTypes(const std::set<Hash> & types)
    { m_types = types; }

    void setMacros(const std::set<Macro> & macros)
    { m_macros = macros; }
    
    std::set<std::string> includes() const
    { return m_includes; }

    std::set<Hash> types() const
    { return m_types; }

    std::set<Macro> macros() const
    { return m_macros; }
    
    PTNode scopePosition() const
    { return m_scope_pos; }

    void setScopePosition(PTNode pos)
    { m_scope_pos = pos; }

    void setText(const std::string & text)
    { m_text = text; }
        
    std::string text() const
    { return m_text; }

    void setFreeVariables(const std::set<VariableInfo> & vars)
    { m_free_vars = vars; }

    std::set<VariableInfo> freeVariables() const
    { return m_free_vars; }

    void setFreeFunctions(const std::set<FunctionInfo> & funs)
    { m_free_funs = funs; }

    std::set<FunctionInfo> freeFunctions() const
    { return m_free_funs; }
    
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
        , m_decl(NULL)
        , m_parent(_parent)
        , m_children()
        , m_class(std::string(_stmt->getStmtClassName()))
        , m_range(_stmt->getSourceRange())
        , m_guard(false)
        , m_scope_pos(NoNode)
        , m_macros()
        , m_text()
        , m_free_vars()
        , m_free_funs()
    {}

    Ast(clang::Decl * _decl, AstRef _parent)
        : m_stmt(NULL)
        , m_decl(_decl)
        , m_parent(_parent)
        , m_children()
        , m_class(std::string(_decl->getDeclKindName()))
        , m_range(_decl->getSourceRange())
        , m_guard(false)
        , m_scope_pos(NoNode)
        , m_macros()
        , m_text()
        , m_free_vars()
        , m_free_funs()
    {}
    
    void add_child(AstRef child)
    { m_children.push_back(child); }
    
private:
    clang::Stmt * m_stmt;
    clang::Decl * m_decl;
    AstRef m_parent;
    std::vector<AstRef> m_children;

    // These are all traits of the Stmt/Decl that do not seem
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
    bool m_guard;
    std::set<Hash> m_types;
    std::set<std::string> m_includes;
    PTNode m_scope_pos;
    std::set<Macro> m_macros;
    std::string m_text;
    std::set<VariableInfo> m_free_vars;
    std::set<FunctionInfo> m_free_funs;
};

class AstTable
{
public:
    AstRef create(clang::Stmt * stmt, AstRef parent)
    {
        asts.push_back(Ast(stmt, parent));
        return asts.size();
    }

    AstRef create(clang::Decl * decl, AstRef parent)
    {
        asts.push_back(Ast(decl, parent));
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


} // namespace clang_mutate

#endif
