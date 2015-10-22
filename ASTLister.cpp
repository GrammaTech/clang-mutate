#include "ASTLister.h"
#include "BinaryAddressMap.hpp"
#include "ASTEntryList.hpp"

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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

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
              bool OutputAsJSON = false)
      : Out(Out ? *Out : llvm::outs()),
        Binary(Binary),
        BinaryAddresses(Binary),
        OutputAsJSON(OutputAsJSON),
        PM(NULL) {}

    ~ASTLister(){
      delete PM;
      PM = NULL;
    }

    virtual void HandleTranslationUnit(ASTContext &Context) {
      TranslationUnitDecl *D = Context.getTranslationUnitDecl();

      // Setup
      Counter=0;

      MainFileID=Context.getSourceManager().getMainFileID();
      MainFileName = Context.getSourceManager().getFileEntryForID(MainFileID)->getName();

      Rewrite.setSourceMgr(Context.getSourceManager(),
                           Context.getLangOpts());

      // Run Recursive AST Visitor
      TraverseDecl(D);
   
      // Output the results   
      if ( OutputAsJSON )
        ASTEntries.toStreamJSON( Out );
      else
        ASTEntries.toStream( Out );
    };

    bool IsSourceRangeInMainFile(SourceRange r)
    {
      FullSourceLoc loc = FullSourceLoc(r.getEnd(), Rewrite.getSourceMgr());
      return (loc.getFileID() == MainFileID);
    }

    // Return true if the clang::Expr is a statement in the C/C++ grammar.
    // This is done by testing if the parent of the clang::Expr
    // is an aggregation type.  The immediate children of an aggregation
    // type are all valid statements in the C/C++ grammar.
    bool IsCompleteCStatement(Stmt *ExpressionStmt)
    {
      Stmt* parent = PM->getParent(ExpressionStmt);

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

    virtual bool VisitDecl(Decl *D){
      // Set a flag if we are in a new declaration
      // section.  There is a tight coupling between
      // this action and VisitStmt(Stmt* ).
      if (D->getKind() == Decl::Function) {
        IsNewFunctionDecl = true; 
      }

      return true;
    }

    virtual bool VisitStmt(Stmt *S){
      SourceRange R;

      // Test if we are in a new function
      // declaration.  If so, update the parent
      // map with the root of this function declaration.
      if ( IsNewFunctionDecl ) {
        delete PM;
        PM = new ParentMap(S);
        IsNewFunctionDecl = false;
      }
 
      switch (S->getStmtClass()){

      // S is the NULL statement, so return.
      case Stmt::NoStmtClass:
        return true;

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
        R = S->getSourceRange();
        if(IsSourceRangeInMainFile(R)){
          if ( BinaryAddresses.isEmpty() ) 
            ASTEntries.addEntry( new ASTNonBinaryEntry(Counter, S, Rewrite) );
          else
            ASTEntries.addEntry( new ASTBinaryEntry(Counter, S, Rewrite, BinaryAddresses) );
        }
        break;

      // These classes of statements may correspond
      // to one or more lines in a source file.
      // If applicable, associate them with binary
      // source code.
      case Stmt::AtomicExprClass:
      case Stmt::CXXMemberCallExprClass:
      case Stmt::CXXOperatorCallExprClass:
      case Stmt::CallExprClass:
        R = S->getSourceRange();
        if(IsSourceRangeInMainFile(R) && 
           IsCompleteCStatement(S)){
          if ( BinaryAddresses.isEmpty() )
            ASTEntries.addEntry( new ASTNonBinaryEntry(Counter, S, Rewrite) );
          else
            ASTEntries.addEntry( new ASTBinaryEntry(Counter, S, Rewrite, BinaryAddresses) );
        }
        break;

      default:
        // These classes of statements correspond
        // to sub-expressions within a C/C++ statement.
        // They are too granular to associate with binary
        // source code. 
        R = S->getSourceRange();
        if (IsSourceRangeInMainFile(R))
          ASTEntries.addEntry( new ASTNonBinaryEntry(Counter, S, Rewrite) );
        break;
      }

      Counter++;
      return true;
    }

  private:
    raw_ostream &Out;
    StringRef Binary;
    bool OutputAsJSON;

    BinaryAddressMap BinaryAddresses;
    ASTEntryList ASTEntries;

    Rewriter Rewrite;
    ParentMap* PM;
    bool IsNewFunctionDecl;
    unsigned int Counter;
    FileID MainFileID;
    std::string MainFileName;
  };
}

clang::ASTConsumer *clang_mutate::CreateASTLister(clang::StringRef Binary, bool OutputAsJSON){
  return new ASTLister(0, Binary, OutputAsJSON);
}
