#ifndef CLANG_MUTATE_REQUIREMENTS_H
#define CLANG_MUTATE_REQUIREMENTS_H

#include "AstRef.h"
#include "Macros.h"
#include "Function.h"
#include "Variable.h"
#include "Scopes.h"
#include "Utils.h"
#include "Bindings.h"
#include "Renaming.h"
#include "SyntacticContext.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"

#include <set>

namespace clang_mutate {

struct TU;

// A traversal to run on each statement in order to gather
// information about the context required by the statement;
// for example, what variables appear free, what macros
// are used, and what #include directives are needed to get
// the appropriate library functions.
class Requirements
  : public clang::ASTConsumer
  , public clang::RecursiveASTVisitor<Requirements>
{
public:
    typedef RecursiveASTVisitor<Requirements> base;

    Requirements(
        TURef _tu,
        clang::ASTContext * astContext,
        SyntacticContext _sctx,
        clang::CompilerInstance * _ci,
        const std::vector<std::vector<std::string> > & scopes);
    
    std::set<VariableInfo> variables()   const;
    std::set<FunctionInfo> functions()   const;
    std::set<std::string>  includes()    const;
    std::set<Hash>         types()       const;
    std::set<Macro>        macros()      const;
    std::string            text()        const;
    AstRef                 parent()      const;
    PTNode                 scopePos()    const;
    clang::SourceRange     sourceRange() const;
    clang::SourceRange     normalizedSourceRange() const;
    clang::PresumedLoc     beginLoc()    const;
    clang::PresumedLoc     endLoc()      const;
    Replacements           replacements() const;
    TURef                  tu()          const;
    SyntacticContext       syn_ctx()     const;

    bool VisitStmt(clang::Stmt * expr);

    bool VisitExplicitCastExpr(clang::ExplicitCastExpr * expr);
    bool VisitUnaryExprOrTypeTraitExpr(
        clang::UnaryExprOrTypeTraitExpr * expr);

    bool VisitVarDecl(clang::VarDecl * decl);
    bool VisitDeclRefExpr(clang::DeclRefExpr * decl);
    
    bool toplevel_is_macro() const { return toplev_is_macro; }

    void setParent(AstRef p)
    { m_parent = p; }

    void setScopePos(PTNode pos)
    { m_scope_pos = pos; }

    void setSourceRange(clang::PresumedLoc pBegin,
                        clang::PresumedLoc pEnd,
                        clang::SourceRange r,
                        clang::SourceRange nr)
    {
        m_begin_ploc = pBegin;
        m_end_ploc = pEnd;
        m_source_range = r;
        m_normalized_source_range = nr;
    }

    void setReplacements(const Replacements & r)
    { m_replacements = r; }
    
    clang::ASTContext * astContext()
    { return m_ast_context; }

    clang::CompilerInstance * CI()
    { return ci; }

private:

    void gatherMacro(clang::Stmt * stmt);
    void addAddlType(const clang::QualType & qt);

    TURef m_tu;
    clang::CompilerInstance * ci;
    clang::ASTContext * m_ast_context;
    SyntacticContext m_syn_ctx;
    std::set<BindingCtx::Binding> m_vars;
    std::set<FunctionInfo> m_funs;
    std::set<std::string> m_includes;
    std::set<Hash> addl_types;
    std::set<Macro> m_macros;
    AstRef m_parent;
    PTNode m_scope_pos;
    clang::PresumedLoc m_begin_ploc;
    clang::PresumedLoc m_end_ploc;
    clang::SourceRange m_source_range;
    clang::SourceRange m_normalized_source_range;
    Replacements m_replacements;
    bool toplev_is_macro, is_first;
    BindingCtx ctx;

    std::map<std::string, size_t> decl_depth;
};


} // end namespace clang_mutate

#endif
