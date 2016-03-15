#include "TU.h"

#include "Bindings.h"
#include "Macros.h"
#include "Renaming.h"
#include "Scopes.h"
#include "Requirements.h"
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
    BuildTU(StringRef Binary,
            StringRef DwarfFilepathMap,
            CompilerInstance * _ci)
        : Binary(Binary)
        , BinaryAddresses(Binary, DwarfFilepathMap)
        , ci(_ci)
        , sm(_ci->getSourceManager())
        , asts(TUs.back().astTable)
        , decl_scopes(TUs.back().scopes)
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
        DeclDepth = 0;
        TraverseDecl(Context.getTranslationUnitDecl());
    }

    void RegisterFunctionDecl(const FunctionDecl * F)
    {
        // TODO, see ASTLister.cpp
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
            if (parent != NoAst && asts[parent].isStmt()) {
                asts[ast].setIsGuard(
                    Utils::IsGuardStmt(s, asts[parent].asStmt()));
            }
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
            bool keep_going = base::TraverseDecl(d);
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
    StringRef Binary;
    BinaryAddressMap BinaryAddresses;
    CompilerInstance * ci;
    SourceManager & sm;
    AstTable & asts;

    std::vector<AstRef> spine;

    unsigned int DeclDepth;
    std::vector<VarScope> var_scopes;
    Scope & decl_scopes;
};

} // namespace clang_mutate

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateTU(clang::StringRef Binary,
                       clang::StringRef DwarfFilepathMap,
                       clang::CompilerInstance * CI)
{
    return std::unique_ptr<clang::ASTConsumer>(
               new BuildTU(Binary,
                           DwarfFilepathMap,
                           CI));
}
