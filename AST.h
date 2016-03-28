#ifndef CLANG_MUTATE_AST_H
#define CLANG_MUTATE_AST_H

#include <vector>
#include <string>

#include "ASTRef.h"
#include "Hash.h"
#include "PointedTree.h"
#include "BinaryAddressMap.h"
#include "Macros.h"
#include "Renaming.h"
#include "Requirements.h"
#include "Renaming.h"

#include "clang/AST/AST.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>

namespace clang_mutate {

struct TU;

class Ast
{
public:
    bool isDecl() const { return (m_decl != NULL); }
    clang::Decl * asDecl() const { return m_decl; }
    
    bool isStmt() const { return (m_stmt != NULL); }
    clang::Stmt * asStmt() const { return m_stmt; }

    // The source range for the statement itself, not including
    // any trailing semicolon.
    clang::SourceRange sourceRange() const
    { return m_range; }

    // The source range for the statement plus any trailing
    // semicolon, if this is a full statement.
    clang::SourceRange normalizedSourceRange() const
    { return m_normalized_range; }

    std::string className() const
    { return m_class; }

    bool isGuard() const
    { return m_guard; }

    void setIsGuard(bool guard)
    { m_guard = guard; }

    void setIncludes(const std::set<std::string> & includes)
    { m_includes = includes; }

    void setTypes(const std::set<Hash> & types)
    { m_types = types; }

    void setMacros(const std::set<Macro> & macros)
    { m_macros = macros; }
    
    std::set<std::string> includes() const
    { return m_includes; }

    std::set<Hash> types() const
    { return m_types; }

    std::set<Macro> macros() const
    { return m_macros; }
    
    PTNode scopePosition() const
    { return m_scope_pos; }

    void setScopePosition(PTNode pos)
    { m_scope_pos = pos; }

    void setReplacements(const Replacements & r)
    { m_replacements = r; }

    std::string opcode() const
    { return m_opcode; }

    void setFreeVariables(const std::set<VariableInfo> & vars)
    { m_free_vars = vars; }

    std::set<VariableInfo> freeVariables() const
    { return m_free_vars; }

    void setFreeFunctions(const std::set<FunctionInfo> & funs)
    { m_free_funs = funs; }

    std::set<FunctionInfo> freeFunctions() const
    { return m_free_funs; }

    void setCanHaveAssociatedBytes(bool yn)
    { m_can_have_bytes = yn; }

    bool canHaveAssociatedBytes() const
    { return m_can_have_bytes; }
    
    std::string srcFilename(TU & tu) const;

    bool binaryAddressRange(
        TU & tu, BinaryAddressMap::BeginEndAddressPair & addrRange) const;

    bool isFullStmt() const
    { return m_full_stmt; }

    void setIsFullStmt(bool yn)
    { m_full_stmt = yn; }
    
    void setFieldDeclProperties(clang::ASTContext * context);

    AstRef counter() const
    { return m_counter; }
    
    AstRef parent() const
    { return m_parent; }

    std::vector<AstRef> children() const
    { return m_children; }
    
    typedef std::vector<AstRef>::const_iterator child_iterator;

    child_iterator begin_children() const
    { return m_children.begin(); }

    child_iterator end_children() const
    { return m_children.end(); }

    picojson::value toJSON(const std::set<std::string> & keys,
                           TU & tu) const;
                           
    Ast(clang::Stmt * _stmt,
        AstRef _counter,
        AstRef _parent,
        clang::SourceRange r,
        clang::SourceRange nr,
        clang::PresumedLoc pBegin,
        clang::PresumedLoc pEnd);

    Ast(clang::Decl * _decl,
        AstRef _counter,
        AstRef _parent,
        clang::SourceRange r,
        clang::SourceRange nr,
        clang::PresumedLoc pBegin,
        clang::PresumedLoc pEnd);
    
    void add_child(AstRef child)
    { m_children.push_back(child); }
    
private:
    clang::Stmt * m_stmt;
    clang::Decl * m_decl;
    AstRef m_counter;
    AstRef m_parent;
    std::vector<AstRef> m_children;

    // These are all traits of the Stmt/Decl that do not seem
    // to be recoverable after the ActionFactory has run.
    // We cache them here; if we can figure out how to
    // get clang to keep the appropriate structures in
    // memory, then we can avoid the caching entirely.
    //
    // To catch things like this, load two or more files
    // into clang-mutate at once and try to process ASTs
    // in the first file. For the last file processed,
    // the relevant clang data structures stay alive,
    // making it harder to notice bugs where we try to
    // access vanished data.
    //
    std::string m_class;    
    clang::SourceRange m_range;
    clang::SourceRange m_normalized_range;
    clang::PresumedLoc m_begin_loc;
    clang::PresumedLoc m_end_loc;
    bool m_guard;
    std::set<Hash> m_types;
    std::set<std::string> m_includes;
    PTNode m_scope_pos;
    std::set<Macro> m_macros;
    std::set<VariableInfo> m_free_vars;
    std::set<FunctionInfo> m_free_funs;
    std::string m_opcode;
    bool m_full_stmt;
    bool m_can_have_bytes;
    Replacements m_replacements;

    // Properties for struct field decls
    bool m_field_decl;
    std::string m_field_name;
    std::string m_base_type;
    bool m_bit_field;
    unsigned m_bit_field_width;
    unsigned long m_array_length;
};

class AstTable
{
public:
    AstRef create(clang::Stmt * stmt, Requirements & reqs)
    { return impl_create(stmt, reqs); }

    AstRef create(clang::Decl * decl, Requirements & reqs)
    { return impl_create(decl, reqs); }

    Ast& ast(AstRef ref);
    Ast& operator[](AstRef ref);

    AstRef nextAstRef() const;
    size_t count() const;

    typedef std::vector<Ast>::iterator iterator;
    iterator begin() { return asts.begin(); }
    iterator end()   { return asts.end();   }

private:
    template <typename T>
        AstRef impl_create(T * clang_obj, Requirements & reqs);

    std::vector<Ast> asts;
};


} // namespace clang_mutate

#endif
