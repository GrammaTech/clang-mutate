#include "ASTBinaryAddressLister.h"
#include "BinaryAddressMap.hpp"

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

  class ASTBinaryAddressLister : public ASTConsumer,
                                 public RecursiveASTVisitor<ASTBinaryAddressLister> {
    typedef RecursiveASTVisitor<ASTBinaryAddressLister> base;

  public:
    ASTBinaryAddressLister(raw_ostream *Out = NULL,
                           StringRef Binary = (StringRef) "")
      : Out(Out ? *Out : llvm::outs()),
        Binary(Binary),
        BinaryAddresses(Binary),
        PM(NULL) {}

    ~ASTBinaryAddressLister(){
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

    void ListStmt(Stmt *S)
    {
      char Msg[256];
      SourceManager &SM = Rewrite.getSourceMgr();
      PresumedLoc BeginPLoc = SM.getPresumedLoc(S->getSourceRange().getBegin());
      PresumedLoc EndPLoc = SM.getPresumedLoc(S->getSourceRange().getEnd());

      if ( BinaryAddresses.isEmpty() ) // no binary information given
      {
        sprintf(Msg, "%8d %6d:%-3d %6d:%-3d %s",
                     Counter,
                     BeginPLoc.getLine(),
                     BeginPLoc.getColumn(),
                     EndPLoc.getLine(),
                     EndPLoc.getColumn(),
                     S->getStmtClassName());
        Out << Msg << "\n";
      }
      else 
      {
        // binary information given
        unsigned long long BeginAddress = 
          BinaryAddresses.getBeginAddressForLine( MainFileName, BeginPLoc.getLine() );
        unsigned long long EndAddress = 
          BinaryAddresses.getEndAddressForLine( MainFileName, EndPLoc.getLine() );

        if ( BeginAddress != ((unsigned long long) -1) &&
             EndAddress != ((unsigned long long) -1))
        {
          // file, linenumber could be found in the binary's debug info
          // print the begin/end address in the text segment
          sprintf(Msg, "%8d %6d:%-3d %6d:%-3d %#016x %#016x %s", 
                        Counter,
                        BeginPLoc.getLine(),
                        BeginPLoc.getColumn(),
                        EndPLoc.getLine(),
                        EndPLoc.getColumn(),
                        BeginAddress,
                        EndAddress,
                        S->getStmtClassName());
          Out << Msg << "\n";
        }
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

      case Stmt::NoStmtClass:
        return true;

      // These classes of statements
      // correspond to exactly 1 or more
      // lines in a source file.
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
          ListStmt(S);
        }
        break;

      // These expression may correspond
      // to one or more lines in a source file.
      case Stmt::AtomicExprClass:
      case Stmt::CXXMemberCallExprClass:
      case Stmt::CXXOperatorCallExprClass:
      case Stmt::CallExprClass:
        R = S->getSourceRange();
        if(IsSourceRangeInMainFile(R) && 
           IsCompleteCStatement(S)){
          ListStmt(S);
        }
        break;
      default:
        break;
      }

      Counter++;
      return true;
    }

  private:
    raw_ostream &Out;
    StringRef Binary;

    BinaryAddressMap BinaryAddresses;

    Rewriter Rewrite;
    ParentMap* PM;
    bool IsNewFunctionDecl;
    unsigned int Counter;
    FileID MainFileID;
    std::string MainFileName;
  };
}

clang::ASTConsumer *clang_mutate::CreateASTBinaryAddressLister(clang::StringRef Binary){
  return new ASTBinaryAddressLister(0, Binary);
}
