// Copyright (C) 2012 Eric Schulte
#include "ASTMutate.h"
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
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Timer.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include <sstream>
#include <fstream>

#define VISIT(func) \
  bool func { VisitRange(element->getSourceRange()); return true; }

namespace clang_mutate{
  using namespace clang;

  class ASTMutator : public ASTConsumer,
                     public RecursiveASTVisitor<ASTMutator> {
    typedef RecursiveASTVisitor<ASTMutator> base;

  public:
    ASTMutator(raw_ostream *p_Out = NULL,
               ACTION p_Action = NUMBER,
               unsigned int p_Stmt1 = 0, 
               unsigned int p_Stmt2 = 0,
               StringRef p_Value1 = StringRef(""),
               StringRef p_Value2 = StringRef(""),
               unsigned int p_Depth = 0)
      : Out(p_Out ? *p_Out : llvm::outs()),
        Action(p_Action), Stmt1(p_Stmt1), Stmt2(p_Stmt2),
        Value1(p_Value1.str()), Value2(p_Value2.str()),
        Depth(p_Depth)
    {}

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
      case SETRANGE:
      {
          SourceRange r(Range1.getBegin(), Range2.getEnd());
          Rewrite.ReplaceText(r, StringRef(Value1));
          break;
      }
      default: break;
      }
      OutputRewritten(Context);
    };
    
    Rewriter Rewrite;

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

    // Normalize source range so the source range returned:
    // 1) Includes the final semi-colon for full statements
    // 2) In the case of a macro argument, is the immediate source
    //    range for the macro argument.
    SourceRange normalizedSourceRange(Stmt * stmt)
    {
        SourceRange r = stmt->getSourceRange();
        SourceRange ret;

        if (stmt == getEnclosingFullStmt()) {
            ret = Utils::expandRange(Rewrite.getSourceMgr(),
                                     Rewrite.getLangOpts(),
                                     r);
        }
        else {
            ret = r;
        }
        ret = Utils::getImmediateMacroArgCallerRange(
                  Rewrite.getSourceMgr(),
                  ret);

        return ret;
    }

    void AnnotateStmt(Stmt *s)
    {
      char label[128];
      unsigned EndOff;
      SourceRange r = normalizedSourceRange(s);
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

    void SetRange(SourceRange r){
      if (Counter == Stmt1)
          Rewrite.ReplaceText(r, StringRef(Value1));
      if (Counter == Stmt2)
          Rewrite.ReplaceText(r, StringRef(Value2));
    }

    void InsertRange(SourceRange r){
      if (Counter == Stmt1) Rewrite.InsertText(r.getBegin(), 
                                               StringRef(Value1),
                                               false);
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
            SourceRange r = normalizedSourceRange(s);
            Rewrite.InsertText(r.getBegin(), 
                               StringRef(Value1), 
                               false);
        }
    }

    void CutEnclosing()
    {
        if (Counter == Stmt1) {
            Stmt * s = getEnclosingFullStmt();
            SourceRange r = normalizedSourceRange(s);

            char label[24];
            sprintf(label, "/* cut-enclosing: %d */", Counter);
            Rewrite.ReplaceText(r, label);
        }
    }
          
    void GetScope()
    {
        if (Counter != Stmt1)
            return;
        std::vector<std::vector<std::string> > names =
            scopes.get_names_in_scope(Depth);
        for (std::vector<std::vector<std::string> >::iterator
                 it = names.begin(); it != names.end(); ++it)
        {
            Out << ">";
            for (std::vector<std::string>::iterator
                     jt = it->begin(); jt != it->end(); ++jt)
            {
                Out << " " << *jt;
            }
            Out << "\n";
        }
    }
      
    bool VisitStmt(Stmt *s){
      if (!Utils::ShouldVisitStmt(Rewrite.getSourceMgr(),
                                  Rewrite.getLangOpts(),
                                  mainFileID,
                                  s))
      {
          return true;
      }
      switch (s->getStmtClass()){
      case Stmt::NoStmtClass:
        break;
      default:
        SourceRange r = normalizedSourceRange(s);
        ++Counter;
        switch(Action){
        case ANNOTATOR:    AnnotateStmt(s); break;
        case NUMBER:       NumberRange(r);  break;
        case CUT:          CutRange(r);     break;
        case SETRANGE:     SaveRange(r);    break;
        case CUTENCLOSING: CutEnclosing();  break;
        case SET:
        case SET2:         SetRange(r);     break;
        case VALUEINSERT:  InsertRange(r);  break;
        case INSERT:
        case SWAP:         SaveRange(r);    break;
        case PREINSERT:    PreInsert();     break;
        case IDS:                           break;
        case GETSCOPE:     GetScope();      break;
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

    bool TraverseDecl(Decl *D){
        bool keep_going;

        if (D != NULL && D->hasBody()) {
            const FunctionDecl * F = D->getAsFunction();

            scopes.enter_scope(F->getBody());
            for (unsigned int i = 0; i < F->getNumParams(); i++) {
                const ParmVarDecl * param = F->getParamDecl(i);
                scopes.declare(param->getIdentifier());
            }

            keep_going = base::TraverseDecl(D);
            scopes.exit_scope();
        }
        else {
            keep_going = base::TraverseDecl(D);
        }

        return keep_going;
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
    std::string Value1, Value2;
    unsigned int Depth;
    unsigned int Counter;
    FileID mainFileID;
    SourceRange Range1, Range2;
    std::string Rewritten1, Rewritten2;
    DeclScope scopes;
    std::vector<std::pair<Stmt*, unsigned int> > spine;
  };
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTNumberer(){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              NUMBER));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTIDS(){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              IDS));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTAnnotator(){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              ANNOTATOR));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTCutter(unsigned int Stmt){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              CUT, 
                              Stmt));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTRangeSetter(unsigned int Stmt1, 
                                   unsigned int Stmt2, 
                                   StringRef Value1){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              SETRANGE, 
                              Stmt1, 
                              Stmt2, 
                              Value1));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTEnclosingCutter(unsigned int Stmt){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              CUTENCLOSING, 
                              Stmt));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTInserter(unsigned int Stmt1, 
                                unsigned int Stmt2){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              INSERT, 
                              Stmt1, 
                              Stmt2));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTSwapper(unsigned int Stmt1, 
                               unsigned int Stmt2){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              SWAP, 
                              Stmt1, 
                              Stmt2));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTSetter(unsigned int Stmt, 
                              clang::StringRef Value1){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              SET, 
                              Stmt, 
                              -1, 
                              Value1));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTSetter2(
    unsigned int Stmt1, clang::StringRef Value1,
    unsigned int Stmt2, clang::StringRef Value2)
{
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              SET2, 
                              Stmt1, 
                              Stmt2, 
                              Value1, 
                              Value2));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTValueInserter(unsigned int Stmt, 
                                     clang::StringRef Value1){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              VALUEINSERT, 
                              Stmt, 
                              -1, 
                              Value1));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTValuePreInserter(unsigned int Stmt, 
                                        clang::StringRef Value1){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              PREINSERT, 
                              Stmt, 
                              -1, 
                              Value1));
}

std::unique_ptr<clang::ASTConsumer> 
clang_mutate::CreateASTScopeGetter(unsigned int Stmt,
                                   unsigned int Depth)
{
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTMutator(0, 
                              GETSCOPE, 
                              Stmt, 
                              -1, 
                              "", 
                              "", 
                              Depth));
}

