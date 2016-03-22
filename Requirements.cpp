
#include "Requirements.h"
#include "TypeDBEntry.h"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang_mutate;
using namespace clang;

Requirements::Requirements(
    CompilerInstance * _ci,
    const std::vector<std::vector<std::string> > & scopes)
    : ci(_ci)
    , m_vars(), m_funs(), m_includes(), addl_types(), m_macros()
    , m_text(""), m_parent(NoAst), m_scope_pos(NoNode)
    , toplev_is_macro(false)
    , is_first(true)
    , ctx()
{
    // Initialize the map from variable name to depth ("how many
    // scopes back was the definition from where we are now?")
    size_t depth = scopes.size() - 1;
    for (auto & scope : reversed(scopes)) {
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

bool Requirements::TraverseVarDecl(VarDecl * decl)
{
  std::string name = decl->getQualifiedNameAsString();
  IdentifierInfo * ident = decl->getIdentifier();

  const Type * tdecl = decl->getTypeSourceInfo()->getType().getTypePtrOrNull();
  Hash type_hash = hash_type(tdecl, ci);

  ctx.push(name, ident, type_hash);
  
  return base::TraverseVarDecl(decl);
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
    if (id != NULL && !ctx.is_bound(name)) {
      if (isa<FunctionDecl>(vdecl)) {
        std::string header;
        if (Utils::in_system_header(vdecl->getLocation(),
                                    ci->getSourceManager(),
                                    header))
        {
          m_includes.insert(header);
        }
        else  {
            m_funs.insert(
                FunctionInfo(static_cast<FunctionDecl*>(vdecl)));
        }
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
    LangOptions & langOpts = ci->getLangOpts();
    
    SourceRange R = stmt->getSourceRange();

    SourceLocation sb = sm.getSpellingLoc(R.getBegin());
    SourceLocation se = sm.getSpellingLoc(R.getEnd())
                                          .getLocWithOffset(1);
    SourceRange sr(sb, se);

    SourceLocation xb = sm.getExpansionRange(R.getBegin()).first;
    SourceLocation xe = sm.getExpansionRange(R.getEnd()).second;
    SourceRange xr(xb, xe);

    std::pair<SourceLocation,SourceLocation> ixbe =
        sm.getImmediateExpansionRange(R.getBegin());
    SourceRange ixr(ixbe.first, ixbe.second);
    
    if (xr == ixr) {
        if (is_first == true)
            toplev_is_macro = true;
        Preprocessor & pp = ci->getPreprocessor();
        StringRef name = pp.getImmediateMacroName(stmt->getLocStart());
        MacroInfo * mi = pp.getMacroInfo(pp.getIdentifierInfo(name));

        if ( mi != NULL ){
            SourceRange def(mi->getDefinitionLoc(), mi->getDefinitionEndLoc());
            
            std::string body;
            SourceLocation end =
                Lexer::getLocForEndOfToken(def.getEnd(), 0, sm, langOpts)
                .getLocWithOffset(1);

            for (SourceLocation it = def.getBegin();
                 it != end;
                 it = it.getLocWithOffset(1))
            {
                body.push_back(sm.getCharacterData(it)[0]);
            }
            while (body.back() == '\n' || body.back() == '\r')
                body.pop_back();

            std::string header;
            if (Utils::in_system_header(mi->getDefinitionLoc(), sm, header)) {
                m_includes.insert(header);
            }
            else {
                m_macros.insert(Macro(name.str(), body));
            }
        }
    }

    is_first = false;
}

void Requirements::addAddlType(const QualType & qt)
{
    Hash type_hash = hash_type(qt.getTypePtrOrNull(), ci);
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

std::set<Macro> Requirements::macros() const
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

std::string Requirements::text() const
{ return m_text; }

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
