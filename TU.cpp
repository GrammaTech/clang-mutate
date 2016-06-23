#include "TU.h"

#include "Bindings.h"
#include "Macros.h"
#include "Renaming.h"
#include "Scopes.h"
#include "Requirements.h"
#include "AuxDB.h"
#include "TypeDBEntry.h"
#include "Utils.h"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/SaveAndRestore.h"

namespace clang_mutate {
using namespace clang;

std::map<TURef, TU*> TUs;

TU * tu_in_progress = NULL;

TU::~TU()
{
    for (auto & ast : asts)
        delete ast;
}

AstRef TU::nextAstRef() const
{ return AstRef(tuid, asts.size() + 1); }

template <class T>
bool is_stmt(T *clang_obj) {
    return false;
}

template <>
bool is_stmt(Stmt *clang_obj) {
    return true;
}

class BuildTU
    : public ASTConsumer
    , public RecursiveASTVisitor<BuildTU>
{
    typedef RecursiveASTVisitor<BuildTU> base;

    typedef std::set<clang::IdentifierInfo*> VarScope;
    
  public:
    BuildTU(TU & _tu, CompilerInstance * _ci)
        : ci(_ci)
        , tu(_tu)
        , sm(_ci->getSourceManager())
        , decl_scopes(_tu.scopes)
        , decls(_tu.aux["decls"])
        , function_starts(_tu.function_starts)
    {}

    ~BuildTU() {}

    virtual void HandleTranslationUnit(ASTContext &Context)
    {
        SourceManager & sm = ci->getSourceManager();
        tu.filename = Utils::safe_realpath(
            sm.getFileEntryForID(sm.getMainFileID())->getName());

        this->Context = &Context;
        spine.clear();
        spine.push_back(NoAst);

        // Add an empty top-level variable scope
        var_scopes.clear();
        var_scopes.push_back(VarScope());
        
        // Run Recursive AST Visitor
        decl_depth = 0;
        TraverseDecl(Context.getTranslationUnitDecl());

        // Register top-level functions
        for (auto & decl : functions)
            processFunctionDecl(decl.first, decl.second);
    }

    // FIXME: better name?
    void processFunctionDecl(AstRef decl_ast, AstRef body_ast)
    {
        assert(decl_ast->isDecl());
        assert(isa<FunctionDecl>(decl_ast->asDecl(*ci)));

        FunctionDecl * F = static_cast<FunctionDecl*>(decl_ast->asDecl(*ci));

        AuxDBEntry &proto = decl_ast->aux();
        proto.set("body", body_ast);
        proto.set("stmt_range", body_ast->stmt_range());

        SourceLocation begin = F->getSourceRange().getBegin();
        SourceOffset offset = sm.getDecomposedLoc(begin).second;
        function_starts[body_ast] = offset;
    }

    template <typename T>
    AstRef makeAst(T * clang_obj, Requirements & required)
    {
        Renames renames(required.variables(),
                        required.functions());
        RenameFreeVar renamer(clang_obj, sm, ci->getLangOpts(), renames);
        required.setText(renamer.getRewrittenString());
    
        AstRef parent = spine.back();
        required.setParent(parent);
        required.setScopePos(decl_scopes.current_scope_position());
        required.setReplacements(renamer.getReplacements());

        bool expand_range = (required.syn_ctx() == SyntacticContext::FullStmt() ||
                             required.syn_ctx() == SyntacticContext::Field() ||
                             required.syn_ctx() == SyntacticContext::UnbracedBody());

        SourceRange sr = clang_obj->getSourceRange();
        SourceRange nsr = Utils::normalizeSourceRange(
            sr,
            expand_range,
            sm,
            ci->getLangOpts());
        required.setSourceRange(sm.getPresumedLoc(sr.getBegin()),
                                sm.getPresumedLoc(sr.getEnd()),
                                sr,
                                nsr);

        AstRef ast = Ast::create(clang_obj, required);

        ast->setInMacroExpansion(clang_obj->getLocStart().isMacroID());

        spine.push_back(ast);
        return ast;
    }

    bool VisitStmt(Stmt * s)
    {
        if (Utils::ShouldVisitStmt(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), s))
        {
            AstRef parent = spine.back();
            SyntacticContext syn_ctx = Utils::is_full_stmt(s, parent)
                ? SyntacticContext::FullStmt()
                : SyntacticContext::Generic();
            Stmt *parent_stmt = parent->isStmt() ? parent->asStmt(*ci) : NULL;

            // Ensure that braces are kept when replacing a CompoundStmt.
            if (isa<CompoundStmt>(s))
                syn_ctx = SyntacticContext::Braced();
            else if (Utils::IsLoopOrIfBody(s, parent_stmt)) {
                syn_ctx = SyntacticContext::UnbracedBody();
            }

            Requirements reqs(tu.tuid,
                              Context,
                              syn_ctx,
                              ci,
                              decl_scopes.get_names_in_scope());
            reqs.TraverseStmt(s);

            AstRef ast = makeAst(s, reqs);
            if (parent != NoAst) {
                if (parent->isStmt()) {
                    ast->setIsGuard(
                        Utils::IsGuardStmt(s, parent_stmt));
                }
                if (ast->className() == "CompoundStmt" &&
                    parent->className() == "Function")
                {
                    functions.push_back(std::make_pair(parent, ast));
                }
            }
        }
        return true;
    }

    bool VisitValueDecl(NamedDecl * d) {
        if (!Utils::ShouldVisitDecl(sm, ci->getLangOpts(),
                                    sm.getMainFileID(), d))
        {
            return true;
        }

        SourceRange r = Utils::normalizeSourceRange(
            d->getSourceRange(),
            true,
            sm,
            ci->getLangOpts());
        if (decl_depth == 1 &&
            d->getIdentifier() != NULL &&
            !isa<FunctionDecl>(d) &&
            Utils::SelectRange(sm, sm.getMainFileID(), r))
        {
            PresumedLoc beginLoc = sm.getPresumedLoc(r.getBegin());
            PresumedLoc endLoc   = sm.getPresumedLoc(r.getEnd());
            
            std::string decl_text = Lexer::getSourceText(
                CharSourceRange::getCharRange(r.getBegin(),
                                              r.getEnd().getLocWithOffset(1)),
                sm, ci->getLangOpts(), NULL);
            std::string decl_name = d->getIdentifier()->getName().str();
            
            AuxDBEntry decl;
            decls.push_back(decl
                            .set("decl_text"     , decl_text)
                            .set("decl_name"     , decl_name)
                            .set("begin_src_line", beginLoc.getLine())
                            .set("begin_src_col" , beginLoc.getColumn())
                            .set("end_src_line"  , endLoc.getLine())
                            .set("end_src_col"   , endLoc.getColumn())
                            .toJSON());
        }
        return true;
    }
    
    bool VisitDecl(Decl * d)
    {
        if (Utils::ShouldVisitDecl(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), d))
        {
            AstRef parent = spine.back();
            if (isa<VarDecl>(d)) {
                decl_scopes.declare(static_cast<VarDecl*>(d)->getIdentifier());
            }
            if (isa<NamedDecl>(d)) {
                std::string name = static_cast<NamedDecl*>(d)->getNameAsString();
                if (parent != NoAst && parent->isStmt()) {
                    parent->addDeclares(name);
                }
            }
            SyntacticContext syn_ctx = SyntacticContext::Generic();

            if (isa<FieldDecl>(d)) {
                syn_ctx = SyntacticContext::Field();
            }
            else if (parent == NoAst)
                syn_ctx = SyntacticContext::TopLevel();

            Requirements reqs(tu.tuid,
                              Context,
                              syn_ctx,
                              ci,
                              decl_scopes.get_names_in_scope());

            reqs.TraverseDecl(d);
            AstRef ast = makeAst(d, reqs);
            if (isa<NamedDecl>(d)) {
                std::string name = static_cast<NamedDecl*>(d)->getNameAsString();
                ast->addDeclares(name);
            }

            if (isa<FunctionDecl>(d)) {
                FunctionDecl * F = static_cast<FunctionDecl*>(d);
                QualType ret = F->getReturnType();

                std::vector<std::pair<std::string, Hash> > args;
                for (unsigned int i = 0; i < F->getNumParams(); ++i) {
                    const ParmVarDecl * p = F->getParamDecl(i);
                    IdentifierInfo * id = p->getIdentifier();
                    args.push_back(
                            std::make_pair(
                                    id ? id->getName().str() : "",
                                    hash_type(p->getType().getTypePtr(), ci)));
                }

                AuxDBEntry &proto = ast->aux();
                proto.set("name", F->getNameAsString());
                proto.set("ret", hash_type(ret.getTypePtr(), ci));
                proto.set("void_ret", ret.getTypePtr()->isVoidType());
                proto.set("args", args);
                proto.set("varargs", F->isVariadic());
            }
        }
        return true;
    }
    
    bool TraverseStmt(Stmt * s)
    {
        if (Utils::ShouldVisitStmt(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), s))
        {
            if (begins_scope(s)) {
                decl_scopes.enter_scope(NoAst);
                var_scopes.push_back(VarScope());
            }
            size_t original_spine_size = spine.size();
            bool keep_going = base::TraverseStmt(s);
            // If we created a new node, remove it from the spine
            // now that the traversal of this Stmt is complete.
            while (spine.size() > original_spine_size)
                spine.pop_back();

            if (begins_scope(s)) {
                var_scopes.pop_back();
                decl_scopes.exit_scope();
            }
            return keep_going;
        }
        else {
            return base::TraverseStmt(s);
        }
    }

    bool TraverseDecl(Decl * d)
    {
        if (Utils::ShouldVisitDecl(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), d))
        {
            if (begins_pseudoscope(d)) {
                decl_scopes.enter_scope(NoAst);
            }
            size_t original_spine_size = spine.size();

            ++decl_depth;
            bool keep_going = base::TraverseDecl(d);
            --decl_depth;
            
            // If we created a new node, remove it from the spine
            // now that the traversal of this Decl is complete.
            while (spine.size() > original_spine_size)
                spine.pop_back();

            if (begins_pseudoscope(d)) {
                decl_scopes.exit_scope();
            }
            return keep_going;
        }
        else {
            return base::TraverseDecl(d);
        }
    }

    bool shouldUseDataRecursionFor(Stmt *S) const {
        // For some nodes RecursiveASTVisitor will traverse with a queue
        // rather than recursing, to avoid stack overflow with deeply-nested
        // trees. Unfortunately, this mode doesn't call the normal Traverse*
        // methods, which can screw up our parent counters. Disable this mode
        // to avoid problems.
        return false;
    }

  private:
    ASTContext * Context;
    CompilerInstance * ci;
    TU & tu;
    SourceManager & sm;

    std::vector<AstRef> spine;

    std::vector<VarScope> var_scopes;
    Scope & decl_scopes;
    std::vector<picojson::value> & decls;
    std::map<AstRef, SourceOffset> & function_starts;
    std::vector<std::pair<AstRef,AstRef> > functions;
    size_t decl_depth;
};

} // namespace clang_mutate

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateTU(clang::CompilerInstance * CI)
{
    return std::unique_ptr<clang::ASTConsumer>
        (new BuildTU(*tu_in_progress, CI));
}
