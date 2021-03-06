#ifndef SCOPES_H
#define SCOPES_H

#include "AstRef.h"
#include "PointedTree.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/AST.h"

#include <vector>
#include <string>

namespace clang_mutate {

  class Scope
  {
  public:
      Scope();
      
      void declare(const clang::IdentifierInfo* id);

      void enter_scope(AstRef scope);
      void exit_scope();

      AstRef get_enclosing_scope(PTNode pos) const;

      PTNode current_scope_position() const;
      void restore_scope_position(PTNode pos);
      
      std::vector<std::vector<std::string> >
          get_names_in_scope_from(PTNode pos, size_t length) const;

      std::vector<std::vector<std::string> >
          get_names_in_scope() const;
  private:
      struct ScopeInfo
      {
      public:
          ScopeInfo(AstRef _scope) : id(""), scope(_scope) {}
          ScopeInfo(const clang::IdentifierInfo* ident)
          : id(ident->getName().str()), scope(NoAst) {}
          
          bool getDeclaration(std::string & result) const
          {
              result = id;
              return id != "";
          }
          
          bool getScope(AstRef & result) const
          {
              result = scope;
              return id == "";
          }

      private:
          std::string id;
          AstRef scope;
      };
      
      PointedTree<ScopeInfo> scopes;
  };

  // Does this statement begin a scope?
  bool begins_scope(clang::Stmt * stmt);

  // Does this decl add something to the following scope
  // (as in ParmVarDecls in a function's definition)?
  bool begins_pseudoscope(clang::Decl * decl);

} // end namespace clang_mutate

#endif
