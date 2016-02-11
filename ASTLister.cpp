#include "ASTLister.h"
#include "BinaryAddressMap.h"
#include "Bindings.h"
#include "ASTEntryList.h"
#include "Macros.h"
#include "Scopes.h"
#include "TypeDBEntry.h"
#include "AuxDB.h"
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

#include <sstream>
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
              unsigned int Stmt1 = -1,
              StringRef Fields = (StringRef) "",
              StringRef Aux = (StringRef) "",
              StringRef Binary = (StringRef) "",
              StringRef DwarfFilepathMap = (StringRef) "",
              bool OutputAsJSON = false,
              CompilerInstance * _CI = NULL)
      : Out(Out ? *Out : llvm::outs()),
        Stmt1(Stmt1),
        Aux(Aux),
        Fields(Fields),
        Binary(Binary),
        BinaryAddresses(Binary, DwarfFilepathMap),
        OutputAsJSON(OutputAsJSON),
        PM(NULL),
        get_bindings(_CI),
        CI(_CI)
    {}

    ~ASTLister(){
      delete PM;
      PM = NULL;
    }

    virtual bool IncludeAux(const std::string & kind)
    {
        if (Aux.str() == "" || Aux.str() == "all")
            return true;
        if (Aux.str() == "none")
            return false;
        std::stringstream ss(Aux.str());
        std::string allowed_kind;
        while (std::getline(ss, allowed_kind, ',')) {
            if (allowed_kind == kind)
                return true;
        }
        return false;
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
      DeclDepth = 0;
      TraverseDecl(D);
   
      // Output the results   
      if ( !ASTEntries.isEmpty() )
      {
        if ( OutputAsJSON ) {
          bool include_types = IncludeAux("types");
          if (Fields.empty()) {
              ASTEntries.toStreamJSON( Out, Stmt1, include_types );
          } else {
            ASTEntries.toStreamJSON( 
              Out,
              Stmt1,
              include_types,
              ASTEntryField::fromJSONNames(Utils::split(Fields.str(), ',')));
          }
        }
        else {
            ASTEntries.toStream( Out, Stmt1 );
        }
      }
    };

    Stmt* GetParentStmt(Stmt *S) {
      Stmt* parent = S;

      do {
        parent = (PM != NULL) ?
                 PM->getParent(parent) :
                 NULL;
      } while (parent != NULL && 
               Spine.find(parent) == Spine.end());

      return parent;
    }

    unsigned int GetNextCounter() {
        return Spine.size() + 1;
    }

    void RegisterFunctionDecl(const FunctionDecl * F)
    {
        Stmt * body = F->getBody();
        if (!body ||
            !F->doesThisDeclarationHaveABody() ||
            !Utils::SelectRange(CI->getSourceManager(),
                                MainFileID,
                                F->getSourceRange()))
        {
            return;
        }

        QualType ret = F->getReturnType();

        std::vector<std::pair<std::string, Hash> > args;
        for (unsigned int i = 0; i < F->getNumParams(); ++i) {
            const ParmVarDecl * p = F->getParamDecl(i);
            args.push_back(std::make_pair(
                               p->getIdentifier()->getName().str(),
                               hash_type(p->getType().getTypePtr(), CI)));
        }

        SourceLocation begin = F->getSourceRange().getBegin();
        SourceLocation end = body->getSourceRange().getBegin();
        std::string decl_text = Lexer::getSourceText(
            CharSourceRange::getCharRange(begin, end),
            CI->getSourceManager(),
            CI->getLangOpts(),
            NULL);

        // Trim trailing whitespace from the declaration
        size_t endpos = decl_text.find_last_not_of(" \t\n\r");
        if (endpos != std::string::npos)
            decl_text = decl_text.substr(0, endpos + 1);
        
        // Build a function prototype, which will be added to the
        // global database. We don't actually need the value here.
        std::ostringstream ss;
        static unsigned int proto_id = 0;
        ss << "proto#" << proto_id++;
        if (IncludeAux("protos")) {
            AuxDB::create(ss.str())
                .set("name", F->getNameAsString())
                .set("text", decl_text)
                .set("body", GetNextCounter())
                .set("ret", hash_type(ret.getTypePtr(), CI))
                .set("void_ret", ret.getTypePtr()->isVoidType())
                .set("args", args)
                .set("varargs", F->isVariadic());
        }
    }

    virtual bool VisitValueDecl(NamedDecl *D){
      // Delete the ParentMap if we are in a new
      // function declaration.  There is a tight 
      // coupling between this action and VisitStmt(Stmt* ).

      if (isa<FunctionDecl>(D)) {
        delete PM;
        PM = NULL;
      }

      SourceManager & sm = CI->getSourceManager();
      const LangOptions & opts = CI->getLangOpts();
      SourceRange r = D->getSourceRange();
      r = Utils::expandRange(sm, opts, r);

      // DeclDepth == 1 is the TranslationUnitDecl,
      // DeclDepth == 2 is its immediate children (global decls)
      if (DeclDepth == 2 &&
          D->getIdentifier() != NULL &&
          Utils::SelectRange(sm, MainFileID, r))
      {
          PresumedLoc beginLoc = sm.getPresumedLoc(r.getBegin());
          PresumedLoc endLoc   = sm.getPresumedLoc(r.getEnd());

          std::string decl_text = Lexer::getSourceText(
              CharSourceRange::getCharRange(r.getBegin(),
                                            r.getEnd().getLocWithOffset(1)),
              sm, opts, NULL);

          std::ostringstream ss;
          static unsigned int decl_id = 0;
          ss << "global-decl#" << decl_id++;
          if (IncludeAux("decls")) {
              AuxDB::create(ss.str())
                  .set("decl_text"     , decl_text)
                  .set("begin_src_line", beginLoc.getLine())
                  .set("begin_src_col" , beginLoc.getColumn())
                  .set("end_src_line"  , endLoc.getLine())
                  .set("end_src_col"   , endLoc.getColumn());
          }
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
      };

      if (Utils::ShouldVisitStmt(Rewrite.getSourceMgr(),
                                 Rewrite.getLangOpts(),
                                 MainFileID,
                                 S))
      { 
        Stmt * P = GetParentStmt(S);
        get_bindings.TraverseStmt(S);
        Spine[S] = GetNextCounter();

        if (!block_spine.empty() && begins_scope(S))
        {
            block_spine.back().first = Spine[S];
        }

        if (Utils::ShouldAssociateBytesWithStmt(S, P)) {
            // This is a full statement, so attempt to associate bytes
            // with it.
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
                    get_bindings.required_types(),
                    decl_scopes.get_names_in_scope(1000));

            ASTEntries.addEntry( NewASTEntry );
        } else {
            // This statement is too granular to have bytes associated
            // with it.
            NewASTEntry =
                new ASTNonBinaryEntry(
                    S,
                    P,
                    Spine,
                    Rewrite,
                    make_renames(get_bindings.free_values(var_scopes),
                                 get_bindings.free_functions()),
                    get_macros.result(),
                    get_bindings.required_types(),
                    decl_scopes.get_names_in_scope(1000));

            ASTEntries.addEntry( NewASTEntry );
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

    bool TraverseVarDecl(VarDecl * decl) {
        decl_scopes.declare(decl->getIdentifier());
        return base::TraverseVarDecl(decl);
    }

    bool TraverseDecl(Decl * D) {
        bool keep_going;
        ++DeclDepth;
        if (D != NULL && D->hasBody()) {
            const FunctionDecl * F = D->getAsFunction();
            RegisterFunctionDecl(F);
            decl_scopes.enter_scope(F->getBody());
            for (unsigned int i = 0; i < F->getNumParams(); i++) {
                const ParmVarDecl * param = F->getParamDecl(i);
                decl_scopes.declare(param->getIdentifier());
            }
            keep_going = base::TraverseDecl(D);
            decl_scopes.exit_scope();
        }
        else {
            keep_going = base::TraverseDecl(D);
        }
        --DeclDepth;
        return keep_going;
    }

    bool TraverseStmt(Stmt * s) {
        bool keep_going;
        if (begins_scope(s)) {
            decl_scopes.enter_scope(s);
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
            decl_scopes.exit_scope();
        }
        else {
            keep_going = base::TraverseStmt(s);
        }
        return keep_going;
    }

  private:
    raw_ostream &Out;
    unsigned int Stmt1;
    StringRef Aux;
    StringRef Fields;
    StringRef Binary;
    BinaryAddressMap BinaryAddresses;

    bool OutputAsJSON;
    ASTEntryList ASTEntries;

    Rewriter Rewrite;
    ParentMap* PM;
    FileID MainFileID;

    std::map<Stmt*, unsigned int> Spine;
    DeclScope decl_scopes;
    std::vector<std::set<clang::IdentifierInfo*> > var_scopes;
    std::vector<
        std::pair<unsigned int,
                  std::vector<unsigned int> > > block_spine;
    GetBindingCtx get_bindings;
    CompilerInstance * CI;

    unsigned int DeclDepth;
  };
}

std::unique_ptr<clang::ASTConsumer>
clang_mutate::CreateASTLister(unsigned int Stmt1, 
                              StringRef Fields,
                              StringRef Aux,
                              clang::StringRef Binary,
                              clang::StringRef DwarfFilepathMap,
                              bool OutputAsJSON,
                              clang::CompilerInstance * CI){
    return std::unique_ptr<clang::ASTConsumer>(
               new ASTLister(0, 
                             Stmt1, 
                             Fields,
                             Aux,
                             Binary, 
                             DwarfFilepathMap,
                             OutputAsJSON, 
                             CI));
}
