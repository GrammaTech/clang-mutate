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

std::vector<TU> TUs;

class BuildTU
    : public ASTConsumer
    , public RecursiveASTVisitor<BuildTU>
{
    typedef RecursiveASTVisitor<BuildTU> base;

    typedef std::set<clang::IdentifierInfo*> VarScope;
    
  public:
    BuildTU(CompilerInstance * _ci)
        : ci(_ci)
        , sm(_ci->getSourceManager())
        , asts(TUs.back().astTable)
        , decl_scopes(TUs.back().scopes)
        , protos(TUs.back().aux["protos"])
        , decls(TUs.back().aux["decls"])
        , function_ranges(TUs.back().function_ranges)
    {}

    ~BuildTU() {}

    virtual void HandleTranslationUnit(ASTContext &Context)
    {
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

    void processFunctionDecl(Decl * d, AstRef body_ast)
    {
        if (!isa<FunctionDecl>(d))
            return;
        FunctionDecl * F = static_cast<FunctionDecl*>(d);
        Stmt * body = F->getBody();
        if (!body ||
            !F->doesThisDeclarationHaveABody() ||
            !Utils::SelectRange(sm, sm.getMainFileID(), F->getSourceRange()))
        {
            return;
        }

        QualType ret = F->getReturnType();

        std::vector<std::pair<std::string, Hash> > args;
        for (unsigned int i = 0; i < F->getNumParams(); ++i) {
            const ParmVarDecl * p = F->getParamDecl(i);
            args.push_back(std::make_pair(
                               p->getIdentifier()->getName().str(),
                               hash_type(p->getType().getTypePtr(), ci)));
        }

        SourceLocation begin = F->getSourceRange().getBegin();
        SourceLocation end = body->getSourceRange().getBegin();
        std::string decl_text = Lexer::getSourceText(
            CharSourceRange::getCharRange(begin, end),
            ci->getSourceManager(),
            ci->getLangOpts(),
            NULL);

        // Trim trailing whitespace from the declaration
        size_t endpos = decl_text.find_last_not_of(" \t\n\r");
        if (endpos != std::string::npos)
            decl_text = decl_text.substr(0, endpos + 1);
        
        // Build a function prototype, which will be added to the
        // global database. We don't actually need the value here.
        AuxDBEntry proto;
        protos.push_back(proto
                         .set("name", F->getNameAsString())
                         .set("text", decl_text)
                         .set("body", body_ast)
                         .set("ret", hash_type(ret.getTypePtr(), ci))
                         .set("void_ret", ret.getTypePtr()->isVoidType())
                         .set("args", args)
                         .set("varargs", F->isVariadic())
                         .toJSON());
        function_ranges[body_ast] =
            SourceRange(begin, body->getSourceRange().getEnd());
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

        SourceRange sr = clang_obj->getSourceRange();
        bool is_full_stmt =
            parent == NoAst ||
            asts[parent].className() == "CompoundStmt";
        SourceRange nsr = Utils::normalizeSourceRange(sr,
                                                      is_full_stmt,
                                                      sm,
                                                      ci->getLangOpts());
        required.setSourceRange(sm.getPresumedLoc(sr.getBegin()),
                                sm.getPresumedLoc(sr.getEnd()),
                                sr,
                                nsr);
        
        AstRef ast = asts.create(clang_obj, required);
        spine.push_back(ast);
        return ast;
    }
    
    bool VisitStmt(Stmt * s)
    {
        if (Utils::ShouldVisitStmt(sm, ci->getLangOpts(),
                                   sm.getMainFileID(), s))
        {
            Requirements reqs(ci, decl_scopes.get_names_in_scope());
            reqs.TraverseStmt(s);
            
            AstRef ast = makeAst(s, reqs);
            AstRef parent = asts[ast].parent();
            if (parent != NoAst) {
                if (asts[parent].isStmt()) {
                    asts[ast].setIsGuard(
                        Utils::IsGuardStmt(s, asts[parent].asStmt()));
                }
                if (asts[parent].className() == "Function") {
                    functions.push_back(std::make_pair(asts[parent].asDecl(),
                                                       ast));
                }
            }
        }
        return true;
    }

    bool VisitNamedDecl(NamedDecl * d) {
        if (!Utils::ShouldVisitDecl(sm, ci->getLangOpts(),
                                    sm.getMainFileID(), d))
        {
            return true;
        }

        SourceRange r = d->getSourceRange();
        if (decl_depth == 1 &&
            d->getIdentifier() != NULL &&
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
            if (isa<VarDecl>(d))
                decl_scopes.declare(static_cast<VarDecl*>(d)->getIdentifier());
            Requirements reqs(ci, decl_scopes.get_names_in_scope());
            reqs.TraverseDecl(d);
#ifdef ALLOW_DECL_ASTS
            makeAst(d, reqs);
#else
            processFunctionDecl(d, asts.nextAstRef());
#endif
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
            PTNode pos;
            if (begins_pseudoscope(d)) {
                pos = decl_scopes.current_scope_position();
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
                decl_scopes.restore_scope_position(pos);
            }
            return keep_going;
        }
        else {
            return base::TraverseDecl(d);
        }
    }

  private:
    CompilerInstance * ci;
    SourceManager & sm;
    AstTable & asts;

    std::vector<AstRef> spine;

    std::vector<VarScope> var_scopes;
    Scope & decl_scopes;
    std::vector<picojson::value> & protos;
    std::vector<picojson::value> & decls;
    std::map<AstRef, SourceRange> & function_ranges;
    std::vector<std::pair<Decl*,AstRef> > functions;
    size_t decl_depth;
};

} // namespace clang_mutate

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateTU(clang::CompilerInstance * CI)
{ return std::unique_ptr<clang::ASTConsumer>(new BuildTU(CI)); }
