#include "ASTLister.h"
#include "BinaryAddressMap.h"
#include "Bindings.h"
#include "ASTEntryList.h"
#include "Macros.h"
#include "Scopes.h"
#include "Utils.h"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/ParentMap.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/SaveAndRestore.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include <iostream>

#define VISIT(func) \
  bool func { VisitRange(element->getSourceRange()); return true; }

namespace clang_mutate{
using namespace clang;

  class ASTLister : public ASTConsumer,
                    public RecursiveASTVisitor<ASTLister> {
    typedef RecursiveASTVisitor<ASTLister> base;

  public:
    ASTLister(raw_ostream *Out = NULL,
              StringRef Binary = (StringRef) "",
              bool OutputAsJSON = false,
              CompilerInstance * _CI = NULL)
      : Out(Out ? *Out : llvm::outs()),
        Binary(Binary),
        BinaryAddresses(Binary),
        OutputAsJSON(OutputAsJSON),
        PM(NULL),
        get_bindings(_CI),
        CI(_CI)
    {}

    ~ASTLister(){
      delete PM;
      PM = NULL;
    }

    virtual void HandleTranslationUnit(ASTContext &Context) {
      TranslationUnitDecl *D = Context.getTranslationUnitDecl();

      // Setup
      MainFileID=Context.getSourceManager().getMainFileID();

      Rewrite.setSourceMgr(Context.getSourceManager(),
                           Context.getLangOpts());

      // Add an empty top-level variable scope
      var_scopes.clear();
      var_scopes.push_back(std::set<IdentifierInfo*>());

      // Run Recursive AST Visitor
      TraverseDecl(D);
   
      // Output the results   
      if ( !ASTEntries.isEmpty() )
      {
        if ( OutputAsJSON ) {
          ASTEntries.toStreamJSON( Out );
        }
        else {
          ASTEntries.toStream( Out );
        }
      }
    };

    Stmt* GetParentStmt(Stmt *S) {
      Stmt* parent = (PM != NULL) ?
                      PM->getParent(S) :
                      NULL;
      return parent;
    }

    // Return true if the clang::Expr is a statement in the C/C++ grammar.
    // This is done by testing if the parent of the clang::Expr
    // is an aggregation type.  The immediate children of an aggregation
    // type are all valid statements in the C/C++ grammar.
    bool IsCompleteCStatement(Stmt *ExpressionStmt)
    {
      Stmt* parent = GetParentStmt(ExpressionStmt);

      if ( parent != NULL ) 
      {
        switch ( parent->getStmtClass() )
        {
        case Stmt::CapturedStmtClass:
        case Stmt::CompoundStmtClass:
        case Stmt::CXXCatchStmtClass:
        case Stmt::CXXForRangeStmtClass:
        case Stmt::CXXTryStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::ForStmtClass:
        case Stmt::IfStmtClass:
        case Stmt::SwitchStmtClass:
        case Stmt::WhileStmtClass: 
          return true;
      
        default:
          return false;
        }
      }

      return false;
    }

    virtual bool VisitDecl(Decl *D){
      // Delete the ParentMap if we are in a new
      // function declaration.  There is a tight 
      // coupling between this action and VisitStmt(Stmt* ).

      if (isa<FunctionDecl>(D)) {
        delete PM;
        PM = NULL;
      }

      return true;
    }
    
    virtual bool VisitStmt(Stmt *S){

      SaveAndRestore<GetBindingCtx> sr(get_bindings);

      ASTEntry* NewASTEntry = NULL;

      GetMacros get_macros(Rewrite.getSourceMgr(),
                           Rewrite.getLangOpts(),
                           CI);
      get_macros.TraverseStmt(S);

      // Test if we are in a new function
      // declaration.  If so, update the parent
      // map with the root statement of this function declaration.
      if ( PM == NULL && S->getStmtClass() == Stmt::CompoundStmtClass ) {
        PM = new ParentMap(S);
      }

      
      if (Utils::ShouldVisitStmt(Rewrite.getSourceMgr(),
                                 Rewrite.getLangOpts(),
                                 MainFileID,
                                 S))
      { 
        Stmt * P = GetParentStmt(S);
        get_bindings.TraverseStmt(S);
        Spine[S] = Spine.size()+1;

        if (!block_spine.empty() && begins_scope(S))
        {
            block_spine.back().first = Spine[S];
        }

        switch (S->getStmtClass()){

        // These classes of statements
        // correspond to exactly 1 or more
        // lines in a source file.  If applicable,
        // associate them with binary source code.
        case Stmt::BreakStmtClass:
        case Stmt::CapturedStmtClass:
        case Stmt::CompoundStmtClass:
        case Stmt::ContinueStmtClass:
        case Stmt::CXXCatchStmtClass:
        case Stmt::CXXForRangeStmtClass:
        case Stmt::CXXTryStmtClass:
        case Stmt::DeclStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::ForStmtClass:
        case Stmt::GotoStmtClass:
        case Stmt::IfStmtClass:
        case Stmt::IndirectGotoStmtClass:
        case Stmt::ReturnStmtClass:
        case Stmt::SwitchStmtClass:
        case Stmt::DefaultStmtClass: 
        case Stmt::CaseStmtClass: 
        case Stmt::WhileStmtClass:

          NewASTEntry = 
            ASTEntryFactory::make(
               S,
               P,
               Spine,
               Rewrite,
               BinaryAddresses,
               make_renames(get_bindings.free_values(var_scopes),
                            get_bindings.free_functions()),
               get_macros.result(),
               get_bindings.required_types());

          ASTEntries.addEntry( NewASTEntry );
          break;

        // These classes of statements may correspond
        // to one or more lines in a source file.
        // If applicable, associate them with binary
        // source code.
        case Stmt::AtomicExprClass:
        case Stmt::CXXMemberCallExprClass:
        case Stmt::CXXOperatorCallExprClass:
        case Stmt::CallExprClass:
          if(IsCompleteCStatement(S))
          {
            NewASTEntry = 
              ASTEntryFactory::make(
                 S,
                 P,
                 Spine,
                 Rewrite,
                 BinaryAddresses,
                 make_renames(get_bindings.free_values(var_scopes),
                              get_bindings.free_functions()),
                 get_macros.result(),
                 get_bindings.required_types());

            ASTEntries.addEntry( NewASTEntry );
          }
          else
          {
            NewASTEntry = 
              new ASTNonBinaryEntry(
                S,
                P,
                Spine,
                Rewrite,
                make_renames(get_bindings.free_values(var_scopes),
                             get_bindings.free_functions()),
                get_macros.result(),
                get_bindings.required_types());
              
            ASTEntries.addEntry( NewASTEntry );
          }

          break;

        // These classes of statements correspond
        // to sub-expressions within a C/C++ statement.
        // They are too granular to associate with binary
        // source code. 
        default:
          NewASTEntry =
              new ASTNonBinaryEntry(
                  S,
                  P,
                  Spine,
                  Rewrite,
                  make_renames(get_bindings.free_values(var_scopes),
                               get_bindings.free_functions()),
                  get_macros.result(),
                  get_bindings.required_types());

          ASTEntries.addEntry( NewASTEntry );
          break;
        }

        if (NewASTEntry != NULL && !block_spine.empty())
        {
            unsigned int counter  = Spine.find(S)->second;
            unsigned int pcounter =
                (P == NULL || Spine.find(P) == Spine.end()) ?
                0 : Spine.find(P)->second;

            if (pcounter == block_spine.back().first) {
                block_spine.back().second.push_back(counter);
            }
        }
      }

      return true;
    }

    bool VisitVarDecl(VarDecl * decl) {
        var_scopes.back().insert(decl->getIdentifier());
        return true;
    }

    bool TraverseStmt(Stmt * s) {
        bool keep_going;
        if (begins_scope(s)) {
            var_scopes.push_back(
                std::set<clang::IdentifierInfo*>());
            block_spine.push_back(
                std::make_pair(0,
                               std::vector<unsigned int>()));
            keep_going = base::TraverseStmt(s);
            unsigned int block_ctr = block_spine.back().first;
            if (block_ctr != 0) {
                ASTEntries.getEntry(block_ctr)
                    ->set_stmt_list(block_spine.back().second);
            }
            block_spine.pop_back();
            var_scopes.pop_back();
        }
        else {
            keep_going = base::TraverseStmt(s);
        }
        return keep_going;
    }

  private:
    raw_ostream &Out;
    StringRef Binary;
    BinaryAddressMap BinaryAddresses;

    bool OutputAsJSON;
    ASTEntryList ASTEntries;

    Rewriter Rewrite;
    ParentMap* PM;
    FileID MainFileID;

    std::map<Stmt*, unsigned int> Spine;
    std::vector<std::set<clang::IdentifierInfo*> > var_scopes;
    std::vector<
        std::pair<unsigned int,
                  std::vector<unsigned int> > > block_spine;
    GetBindingCtx get_bindings;
    CompilerInstance * CI;
  };
}

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateASTLister(clang::StringRef Binary,
                              bool OutputAsJSON,
                              clang::CompilerInstance * CI){
    return std::unique_ptr<clang::ASTConsumer>(new ASTLister(0, Binary, OutputAsJSON, CI));
}
