#ifndef GET_MACROS_H
#define GET_MACROS_H

#include "Json.h"

#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"

#include <string>
#include <stack>
#include <map>
#include <set>

namespace clang_mutate {

class Macro
{
public:
    Macro(const std::string & n,
          const std::string & b)
        : m_name(n)
        , m_body(b)
    {}

    bool operator<(const Macro & other) const
    {
        if (m_name != other.m_name)
            return m_name < other.m_name;
        return m_body < other.m_body;
    }

    std::string name() const { return m_name; }
    std::string body() const { return m_body; }

private:
    std::string m_name;
    std::string m_body;
};

typedef std::set<Macro> Macros;

class GetMacros
    : public clang::ASTConsumer
    , public clang::RecursiveASTVisitor<GetMacros>
{
  typedef RecursiveASTVisitor<GetMacros> Base;

public:

  GetMacros(clang::SourceManager & _sm,
            const clang::LangOptions & _langOpts,
            clang::CompilerInstance * _CI)
      : sm(_sm)
      , macros()
      , langOpts(_langOpts)
      , CI(_CI)
      , toplev_is_macro(false)
      , is_first(true)
  {}

  GetMacros(const GetMacros & m)
      : sm(m.sm)
      , macros(m.macros)
      , langOpts(m.langOpts)
      , CI(m.CI)
      , toplev_is_macro(m.toplev_is_macro)
      , is_first(m.is_first)
  {}

  bool VisitStmt(clang::Stmt * stmt);

  Macros result() const { return macros; }

  std::set<std::string> required_includes() const { return includes; }

  bool toplevel_is_macro() const { return toplev_is_macro; }  

private:
  clang::SourceManager & sm;
  Macros macros;
  std::set<std::string> includes;
  const clang::LangOptions & langOpts;
  clang::CompilerInstance * CI;
  bool toplev_is_macro, is_first;
};

} // end namespace clang_mutate

template <> inline
picojson::value to_json(const clang_mutate::Macro & m)
{ return to_json(std::make_pair(m.name(), m.body())); }


#endif
