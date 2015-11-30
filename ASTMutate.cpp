// Copyright (C) 2012 Eric Schulte
#include "ASTMutate.h"
#include "Scopes.h"

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

  class ASTMutator : public ASTConsumer,
                     public RecursiveASTVisitor<ASTMutator> {
    typedef RecursiveASTVisitor<ASTMutator> base;

  public:
    ASTMutator(raw_ostream *Out = NULL,
               ACTION Action = NUMBER,
               unsigned int Stmt1 = 0, unsigned int Stmt2 = 0,
               StringRef Value = (StringRef)"",
               unsigned int Depth = 0)
      : Out(Out ? *Out : llvm::outs()),
        Action(Action), Stmt1(Stmt1), Stmt2(Stmt2),
        Value(Value), Depth(Depth) {}

    virtual void HandleTranslationUnit(ASTContext &Context) {
      TranslationUnitDecl *D = Context.getTranslationUnitDecl();
      // Setup
      Counter=0;
      mainFileID=Context.getSourceManager().getMainFileID();
      Rewrite.setSourceMgr(Context.getSourceManager(),
                           Context.getLangOpts());
      // Run Recursive AST Visitor
      TraverseDecl(D);
      // Finish Up
      switch(Action){
      case INSERT:
        Rewritten2 = Rewrite.getRewrittenText(Range2);
        Rewrite.InsertText(Range1.getBegin(), (Rewritten2+" "), true);
        break;
      case SWAP:
        {
            SourceManager & sm = Context.getSourceManager();
            unsigned r1_lo = sm.getFileOffset(Range1.getBegin());
            unsigned r1_hi = sm.getFileOffset(Range1.getEnd());
            unsigned r2_lo = sm.getFileOffset(Range2.getBegin());
            unsigned r2_hi = sm.getFileOffset(Range2.getEnd());
            if (r1_lo > r2_hi || r2_lo > r1_hi) {
                Rewritten1 = Rewrite.getRewrittenText(Range1);
                Rewritten2 = Rewrite.getRewrittenText(Range2);
                Rewrite.ReplaceText(Range1, Rewritten2);
                Rewrite.ReplaceText(Range2, Rewritten1);
            }
            else {
                // Error? Warning? What should we do if the user
                // asks us to swap two overlapping ranges?
                llvm::errs() << "ERROR: overlapping ranges "
                             << "[" << r1_lo << ".." << r1_hi << "], "
                             << "[" << r2_lo << ".." << r2_hi << "] "
                             << "in a swap.\n";
                exit(EXIT_FAILURE);
            }
        }
        break;
      default: break;
      }
      OutputRewritten(Context);
    };
    
    Rewriter Rewrite;

    bool SelectRange(SourceRange r)
    {
      FullSourceLoc loc = FullSourceLoc(r.getEnd(), Rewrite.getSourceMgr());
      return (loc.getFileID() == mainFileID);
    }

    void NumberRange(SourceRange r)
    {
      char label[24];
      unsigned EndOff;
      SourceLocation END = r.getEnd();

      sprintf(label, "/* %d[ */", Counter);
      Rewrite.InsertText(r.getBegin(), label, false);
      sprintf(label, "/* ]%d */", Counter);
  
      // Adjust the end offset to the end of the last token, instead
      // of being the start of the last token.
      EndOff = Lexer::MeasureTokenLength(END,
                                         Rewrite.getSourceMgr(),
                                         Rewrite.getLangOpts());

      Rewrite.InsertText(END.getLocWithOffset(EndOff), label, true);
    }

    void AnnotateStmt(Stmt *s)
    {
      char label[128];
      unsigned EndOff;
      SourceRange r = expandRange(s->getSourceRange());
      SourceLocation END = r.getEnd();

      sprintf(label, "/* %d:%s[ */", Counter, s->getStmtClassName());
      Rewrite.InsertText(r.getBegin(), label, false);
      sprintf(label, "/* ]%d */", Counter);

      // Adjust the end offset to the end of the last token, instead
      // of being the start of the last token.
      EndOff = Lexer::MeasureTokenLength(END,
                                         Rewrite.getSourceMgr(),
                                         Rewrite.getLangOpts());

      Rewrite.InsertText(END.getLocWithOffset(EndOff), label, true);
    }

    void GetStmt(Stmt *s){
      if (Counter == Stmt1) Out << Rewrite.ConvertToString(s) << "\n";
    }

    void SetRange(SourceRange r){
      if (Counter == Stmt1) Rewrite.ReplaceText(r, Value);
    }

    void InsertRange(SourceRange r){
      if (Counter == Stmt1) Rewrite.InsertText(r.getBegin(), Value, false);
    }

    void CutRange(SourceRange r)
    {
      char label[24];
      if(Counter == Stmt1) {
        sprintf(label, "/* cut:%d */", Counter);
        Rewrite.ReplaceText(r, label);
      }
    }

    void SaveRange(SourceRange r)
    {
      if (Counter == Stmt1) Range1 = r;
      if (Counter == Stmt2) Range2 = r;
    }

    Stmt * getEnclosingFullStmt(unsigned int * counter = NULL) const
    {
        // Walk back up to the first ancestor of this
        // statement that is a child of a block.
        std::vector<std::pair<Stmt*, unsigned int> >
            ::const_reverse_iterator it = spine.rbegin();
        Stmt * s = it->first;
        unsigned int c = it->second;
        while (it != spine.rend() && it->first != scopes.current_scope()) {           
            s = it->first;
            c = it->second;
            ++it;
        }
        if (counter != NULL)
            *counter = c;
        return s;
    }
      
    void PreInsert()
    {
        if (Counter == Stmt1) {
            Stmt * s = getEnclosingFullStmt();
            SourceRange r = expandRange(s->getSourceRange());
            Rewrite.InsertText(r.getBegin(), Value, false);
        }
    }

    void CutEnclosing()
    {
        if (Counter == Stmt1) {
            Stmt * s = getEnclosingFullStmt();
            SourceRange r = expandRange(s->getSourceRange());

            char label[24];
            sprintf(label, "/* cut-enclosing: %d */", Counter);
            Rewrite.ReplaceText(r, label);
        }
    }
          
    void GetInfo(Stmt * s)
    {
        if (Counter == Stmt1) {
            Out << s->getStmtClassName() << "\n";
            SourceLocation e = findSemiAfterLocation(
                s->getSourceRange().getEnd(), -1);
            Out << (e.isInvalid() ? "no-semi" : "has-semi") << "\n";
            unsigned int ec;
            (void) getEnclosingFullStmt(&ec);
            Out << ec << "\n";
        }
    }

    void GetScope()
    {
        if (Counter != Stmt1)
            return;
        std::vector<std::string> names = scopes.get_names_in_scope(Depth);
        for (std::vector<std::string>::iterator it = names.begin();
             it != names.end();
             ++it)
        {
            Out << *it << "\n";
        }
    }
      
    // This function adapted from clang/lib/ARCMigrate/Transforms.cpp
    SourceLocation findSemiAfterLocation(SourceLocation loc, int Offset = 0) {
      SourceManager &SM = Rewrite.getSourceMgr();
      if (loc.isMacroID()) {
        if (!Lexer::isAtEndOfMacroExpansion(loc, SM,
                                            Rewrite.getLangOpts(), &loc))
          return SourceLocation();
      }
      loc = Lexer::getLocForEndOfToken(loc, Offset, SM,
                                       Rewrite.getLangOpts());

      // Break down the source location.
      std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(loc);

      // Try to load the file buffer.
      bool invalidTemp = false;
      StringRef file = SM.getBufferData(locInfo.first, &invalidTemp);
      if (invalidTemp)
        return SourceLocation();

      const char *tokenBegin = file.data() + locInfo.second;

      // Lex from the start of the given location.
      Lexer lexer(SM.getLocForStartOfFile(locInfo.first),
                  Rewrite.getLangOpts(),
                  file.begin(), tokenBegin, file.end());
      Token tok;
      lexer.LexFromRawLexer(tok);
      if (tok.isNot(tok::semi))
        return SourceLocation();
      return tok.getLocation();
    }

    SourceRange expandRange(SourceRange r){
      // If the range is a full statement, and is followed by a
      // semi-colon then expand the range to include the semicolon.
      SourceLocation b = r.getBegin();
      SourceLocation e = findSemiAfterLocation(r.getEnd());
      if (e.isInvalid()) e = r.getEnd();
      return SourceRange(b,e);
    }

    bool VisitStmt(Stmt *s){
      switch (s->getStmtClass()){
      case Stmt::NoStmtClass:
        break;
      default:
        SourceRange r = expandRange(s->getSourceRange());
        if(SelectRange(r)){
          switch(Action){
          case ANNOTATOR:    AnnotateStmt(s); break;
          case NUMBER:       NumberRange(r);  break;
          case CUT:          CutRange(r);     break;
          case CUTENCLOSING: CutEnclosing();  break;
          case GET:          GetStmt(s);      break;
          case SET:          SetRange(r);     break;
          case VALUEINSERT:  InsertRange(r);  break;
          case INSERT:
          case SWAP:         SaveRange(r);    break;
          case PREINSERT:    PreInsert();     break;
          case IDS:                           break;
          case GETINFO:      GetInfo(s);      break;
          case GETSCOPE:     GetScope();      break;
          }
          Counter++;
        }
      }
      return true;
    }

    bool TraverseStmt(Stmt * s) {
        bool keep_going;
        spine.push_back(std::make_pair(s, Counter));
        if (begins_scope(s)) {
            scopes.enter_scope(s);
            keep_going = base::TraverseStmt(s);
            scopes.exit_scope();
        }
        else {
            keep_going = base::TraverseStmt(s);
        }
        spine.pop_back();
        return keep_going;
    }

    bool TraverseVarDecl(VarDecl * decl) {
        scopes.declare(decl->getIdentifier());
        return base::TraverseVarDecl(decl);
    }
      
    //// from AST/EvaluatedExprVisitor.h
    // VISIT(VisitDeclRefExpr(DeclRefExpr *element));
    // VISIT(VisitOffsetOfExpr(OffsetOfExpr *element));
    // VISIT(VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *element));
    // VISIT(VisitExpressionTraitExpr(ExpressionTraitExpr *element));
    // VISIT(VisitBlockExpr(BlockExpr *element));
    // VISIT(VisitCXXUuidofExpr(CXXUuidofExpr *element));
    // VISIT(VisitCXXNoexceptExpr(CXXNoexceptExpr *element));
    // VISIT(VisitMemberExpr(MemberExpr *element));
    // VISIT(VisitChooseExpr(ChooseExpr *element));
    // VISIT(VisitDesignatedInitExpr(DesignatedInitExpr *element));
    // VISIT(VisitCXXTypeidExpr(CXXTypeidExpr *element));
    // VISIT(VisitStmt(Stmt *element));

    //// from Analysis/Visitors/CFGRecStmtDeclVisitor.h
    // VISIT(VisitDeclRefExpr(DeclRefExpr *element)); // <- duplicate above
    // VISIT(VisitDeclStmt(DeclStmt *element));
    // VISIT(VisitDecl(Decl *element)); // <- throws assertion error
    // VISIT(VisitCXXRecordDecl(CXXRecordDecl *element));
    // VISIT(VisitChildren(Stmt *element));
    // VISIT(VisitStmt(Stmt *element)); // <- duplicate above
    // VISIT(VisitCompoundStmt(CompoundStmt *element));
    // VISIT(VisitConditionVariableInit(Stmt *element));

    void OutputRewritten(ASTContext &Context) {
      // output rewritten source code or ID count
      switch(Action){
      case IDS: Out << Counter << "\n";
      case GET:
      case GETINFO:
      case GETSCOPE:
        break;
      default:
        const RewriteBuffer *RewriteBuf = 
          Rewrite.getRewriteBufferFor(Context.getSourceManager().getMainFileID());
        Out << std::string(RewriteBuf->begin(), RewriteBuf->end());
      }
    }
    
  private:
    raw_ostream &Out;
    ACTION Action;
    unsigned int Stmt1, Stmt2;
    StringRef Value;
    unsigned int Depth;
    unsigned int Counter;
    FileID mainFileID;
    SourceRange Range1, Range2;
    std::string Rewritten1, Rewritten2;
    DeclScope scopes;
    std::vector<std::pair<Stmt*, unsigned int> > spine;
  };
}

clang::ASTConsumer *clang_mutate::CreateASTNumberer(){
  return new ASTMutator(0, NUMBER);
}

clang::ASTConsumer *clang_mutate::CreateASTIDS(){
  return new ASTMutator(0, IDS);
}

clang::ASTConsumer *clang_mutate::CreateASTAnnotator(){
  return new ASTMutator(0, ANNOTATOR);
}

clang::ASTConsumer *clang_mutate::CreateASTCutter(unsigned int Stmt){
  return new ASTMutator(0, CUT, Stmt);
}

clang::ASTConsumer *clang_mutate::CreateASTEnclosingCutter(unsigned int Stmt){
  return new ASTMutator(0, CUTENCLOSING, Stmt);
}

clang::ASTConsumer *clang_mutate::CreateASTInserter(unsigned int Stmt1, unsigned int Stmt2){
  return new ASTMutator(0, INSERT, Stmt1, Stmt2);
}

clang::ASTConsumer *clang_mutate::CreateASTSwapper(unsigned int Stmt1, unsigned int Stmt2){
  return new ASTMutator(0, SWAP, Stmt1, Stmt2);
}

clang::ASTConsumer *clang_mutate::CreateASTGetter(unsigned int Stmt){
  return new ASTMutator(0, GET, Stmt);
}

clang::ASTConsumer *clang_mutate::CreateASTSetter(unsigned int Stmt, clang::StringRef Value){
  return new ASTMutator(0, SET, Stmt, -1, Value);
}

clang::ASTConsumer *clang_mutate::CreateASTValueInserter(unsigned int Stmt, clang::StringRef Value){
    return new ASTMutator(0, VALUEINSERT, Stmt, -1, Value);
}

clang::ASTConsumer *clang_mutate::CreateASTValuePreInserter(unsigned int Stmt, clang::StringRef Value){
    return new ASTMutator(0, PREINSERT, Stmt, -1, Value);
}

clang::ASTConsumer *clang_mutate::CreateASTInfoGetter(unsigned int Stmt)
{
    return new ASTMutator(0, GETINFO, Stmt);
}


clang::ASTConsumer *clang_mutate::CreateASTScopeGetter(unsigned int Stmt,
                                                       unsigned int Depth)
{
    return new ASTMutator(0, GETSCOPE, Stmt, -1, "", Depth);
}

