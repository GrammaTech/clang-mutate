#ifndef SCOPES_H
#define SCOPES_H

#include "clang/Basic/LLVM.h"
#include "clang/AST/AST.h"

#include <vector>
#include <string>

namespace clang_mutate {

  class DeclScope
  {
  public:
      DeclScope();

      void declare(const clang::IdentifierInfo* id);

      void enter_scope(clang::Stmt * stmt);
      void exit_scope();

      clang::Stmt * current_scope() const;

      std::vector<std::vector<std::string> >
          get_names_in_scope(size_t length) const;
    
  private:
    typedef std::pair<clang::Stmt*,
                      std::vector<const clang::IdentifierInfo*> > BlockScope;
     std::vector<BlockScope> scopes;
  };

  bool begins_scope(clang::Stmt * stmt);

} // end namespace clang_mutate

#endif
