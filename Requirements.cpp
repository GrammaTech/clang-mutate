
#include "Requirements.h"
#include "Reversed.h"
#include "TypeDBEntry.h"
#include "TU.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang_mutate;
using namespace clang;

Requirements::Requirements(
    TURef _tu,
    clang::ASTContext * astContext,
    SyntacticContext _sctx,
    clang::CompilerInstance * _ci,
    const std::vector<std::vector<std::string> > & scopes)
    : m_tu(_tu)
    , ci(_ci)
    , m_ast_context(astContext)
    , m_syn_ctx(_sctx)
    , m_vars(), m_funs(), m_includes(), addl_types(), m_macros()
    , m_parent(NoAst), m_scope_pos(NoNode)
    , toplev_is_macro(false)
    , is_first(true)
    , ctx()
{
    // Initialize the map from variable name to depth ("how many
    // scopes back was the definition from where we are now?")
    size_t depth = scopes.size() - 1;
    for (auto & scope : Utils::reversed(scopes)) {
        for (auto & var : scope) {
            decl_depth[var] = depth;
        }
        --depth;
    }
}

///////////////////////////////////////////////////////
//
//  Traversal
//

bool Requirements::VisitVarDecl(VarDecl * decl)
{
  std::string name = decl->getQualifiedNameAsString();
  IdentifierInfo * ident = decl->getIdentifier();

  const QualType tdecl = decl->getTypeSourceInfo()->getType();
  Hash type_hash = hash_type(tdecl, ci, astContext());

  ctx.push(name, ident, type_hash);

  return base::VisitVarDecl(decl);
}

bool Requirements::VisitTypedefDecl(TypedefDecl * decl)
{
  std::string name = decl->getQualifiedNameAsString();
  IdentifierInfo * ident = decl->getIdentifier();

  const QualType tdecl = decl->getTypeSourceInfo()->getType();
  Hash type_hash = hash_type(tdecl, ci, astContext());

  ctx.push(name, ident, type_hash);
  return base::VisitTypedefDecl(decl);
}

bool Requirements::VisitStmt(Stmt * stmt)
{
    gatherMacro(stmt);
    return base::VisitStmt(stmt);
}
    
bool Requirements::VisitDeclRefExpr(DeclRefExpr * expr)
{
    DeclRefExpr * declref = static_cast<DeclRefExpr*>(expr);
    ValueDecl * vdecl     = declref->getDecl();
    std::string name      = vdecl->getQualifiedNameAsString();
    IdentifierInfo * id   = vdecl->getIdentifier();
    bool isMacro = Utils::contains_macro(expr, ci->getSourceManager()) &&
                   vdecl->getLocStart().isMacroID();

    if (id != NULL && !ctx.is_bound(name) && !isMacro) {
        std::string header;
        if (Utils::in_header(vdecl->getLocation(), ci, header)) {
            m_includes.insert(header);
        }
        else if (isa<FunctionDecl>(vdecl)) {
            m_funs.insert(
                FunctionInfo(static_cast<FunctionDecl*>(vdecl)));
        }
        else {
            m_vars.insert(BindingCtx::Binding(name, id));
        }
    }
    return base::VisitDeclRefExpr(expr);
}


bool Requirements::VisitUnaryExprOrTypeTraitExpr(
    UnaryExprOrTypeTraitExpr * expr)
{
    if (expr->isArgumentType())
        addAddlType(expr->getArgumentType());
    return base::VisitUnaryExprOrTypeTraitExpr(expr);
}

bool Requirements::VisitExplicitCastExpr(
    ExplicitCastExpr * expr)
{
    addAddlType(expr->getTypeAsWritten());
    return base::VisitExplicitCastExpr(expr);
}

///////////////////////////////////////////////////////
//
//  Helpers
//

void Requirements::gatherMacro(Stmt * stmt)
{
    SourceManager & sm = ci->getSourceManager();

    if (Utils::contains_macro(stmt, sm)) {
        if (is_first == true)
            toplev_is_macro = true;
        Preprocessor & pp = ci->getPreprocessor();
        StringRef name = pp.getImmediateMacroName(stmt->getLocStart());
        MacroInfo * mi = pp.getMacroInfo(pp.getIdentifierInfo(name));

        if ( mi != NULL ){
            SourceLocation sb = sm.getSpellingLoc(
                                  sm.getImmediateExpansionRange(
                                    stmt->getSourceRange().getBegin()).first);
            SourceLocation mb = mi->getDefinitionLoc();
            std::string header;

            if (MacroDB::getInstance(ci).find(sm.getPresumedLoc(sb)) ||
                MacroDB::getInstance(ci).find(sm.getPresumedLoc(mb))) {
                const Macro* m =
                    MacroDB::getInstance(ci).find(sm.getPresumedLoc(sb)) ?
                    MacroDB::getInstance(ci).find(sm.getPresumedLoc(sb)) :
                    MacroDB::getInstance(ci).find(sm.getPresumedLoc(mb));
                m_macros.insert(m->hash());
            }
            else if (Utils::in_header(sb, ci, header) ||
                     Utils::in_header(mb, ci, header)) {
                m_includes.insert(header);
            }
        }
    }

    is_first = false;
}

void Requirements::addAddlType(const QualType & qt)
{
    Hash type_hash = hash_type(qt, ci, astContext());
    if (type_hash != 0)
        addl_types.insert(type_hash);
}

///////////////////////////////////////////////////////
//
//  Getters
//

std::set<FunctionInfo> Requirements::functions() const
{ return m_funs; }

std::set<std::string> Requirements::includes() const
{ return m_includes; }

std::set<Hash> Requirements::macros() const
{ return m_macros; }

std::set<Hash> Requirements::types() const
{
    std::set<Hash> ans = ctx.required_types();
    for (auto t : addl_types)
        ans.insert(t);
    return ans;
}

std::set<VariableInfo> Requirements::variables() const
{
    std::set<VariableInfo> ans;
    for (auto var : m_vars) {
        std::string name = var.second->getName().str();
        auto search = decl_depth.find(name);
        
        ans.insert(VariableInfo(
                       var.second,
                       search == decl_depth.end()
                       ? 0
                       : search->second));
    }
    return ans;
}

AstRef Requirements::parent() const
{ return m_parent; }

PTNode Requirements::scopePos() const
{ return m_scope_pos; }

SourceRange Requirements::sourceRange() const
{ return m_source_range; }

SourceRange Requirements::normalizedSourceRange() const
{ return m_normalized_source_range; }

clang::PresumedLoc Requirements::beginLoc() const
{ return m_begin_ploc; }

clang::PresumedLoc Requirements::endLoc() const
{ return m_end_ploc; }

Replacements Requirements::replacements() const
{ return m_replacements; }

TURef Requirements::tu() const
{ return m_tu; }

SyntacticContext Requirements::syn_ctx() const
{ return m_syn_ctx; }
