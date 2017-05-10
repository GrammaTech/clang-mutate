#ifndef CLANG_MUTATE_AST_H
#define CLANG_MUTATE_AST_H

#include <vector>
#include <string>

#include "AstRef.h"
#include "AuxDB.h"
#include "Hash.h"
#include "PointedTree.h"
#include "BinaryAddressMap.h"
#include "LLVMInstructionMap.h"
#include "Macros.h"
#include "Optional.h"
#include "Renaming.h"
#include "Requirements.h"
#include "Renaming.h"
#include "SyntacticContext.h"

#include "clang/AST/AST.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>

namespace clang_mutate {

struct TU;

typedef int SourceOffset;
extern const SourceOffset BadOffset;

class Ast
{
public:
    // NOTE: the CompilerInstance is not used here; it is just
    //       a token used to assert "yes, the clang IR backing
    //       this Ast is still in memory".  In effect, these
    //       methods can only be used during TU construction.
    clang::Decl * asDecl(clang::CompilerInstance const&) const
    { return m_decl; }
    clang::Stmt * asStmt(clang::CompilerInstance const&) const
    { return m_stmt; }

    bool isDecl() const { return (m_decl != NULL); }
    bool isStmt() const { return (m_stmt != NULL); }

    // The source range for the statement itself, not including
    // any trailing semicolon.
    clang::SourceRange sourceRange() const
    { return m_range; }

    // The source range for the statement plus any trailing
    // semicolon, if this is a full statement.
    clang::SourceRange normalizedSourceRange() const
    { return m_normalized_range; }

    SourceOffset initial_normalized_offset() const
    { return m_norm_start_off; }

    SourceOffset final_normalized_offset() const
    { return m_norm_end_off; }

    SourceOffset initial_offset() const
    { return m_start_off; }

    SourceOffset final_offset() const
    {
        // Adjustment to compensate for the fact that clang's
        // source ranges include trailing semicolons for DeclStmts,
        // but (seemingly?) not for other statements?
        if (m_declares.empty() || !isFullStmt())
            return m_end_off;
        return m_end_off - 1;
    }

    std::string className() const
    { return m_class; }

    std::vector<std::string> declares() const
    { return m_declares; }

    void addDeclares(const std::string & decl)
    { m_declares.push_back(decl); }

    bool isGuard() const
    { return m_guard; }

    void setIsGuard(bool guard)
    { m_guard = guard; }

    void setIncludes(const std::set<std::string> & includes)
    { m_includes = includes; }

    void setTypes(const std::set<Hash> & types)
    { m_types = types; }

    void setExprType(Hash type)
    { m_expr_type = type; }

    void setMacros(const std::set<Macro> & macros)
    { m_macros = macros; }

    std::set<std::string> includes() const
    { return m_includes; }

    std::set<Hash> types() const
    { return m_types; }

    Hash expr_type() const
    { return m_expr_type; }

    std::set<Macro> macros() const
    { return m_macros; }

    PTNode scopePosition() const
    { return m_scope_pos; }

    void setScopePosition(PTNode pos)
    { m_scope_pos = pos; }

    void setReplacements(const Replacements & r)
    { m_replacements = r; }

    const Replacements & replacements() const
    { return m_replacements; }

    std::string opcode() const
    { return m_opcode; }

    SyntacticContext syntacticContext() const
    { return m_syn_ctx; }

    void setSyntacticContext(SyntacticContext syn_ctx)
    { m_syn_ctx = syn_ctx; }

    void setFreeVariables(const std::set<VariableInfo> & vars)
    { m_free_vars = vars; }

    std::set<VariableInfo> freeVariables() const
    { return m_free_vars; }

    void setFreeFunctions(const std::set<FunctionInfo> & funs)
    { m_free_funs = funs; }

    std::set<FunctionInfo> freeFunctions() const
    { return m_free_funs; }

    void setCanHaveCompilationData(bool yn)
    { m_can_have_compilation_data = yn; }

    bool canHaveCompilationData() const
    { return m_can_have_compilation_data; }

    std::string srcFilename() const;

    bool has_bytes() const;

    bool has_llvm_ir() const;

    Utils::Optional<AddressRange> binaryAddressRange() const;

    Utils::Optional<Bytes> bytes() const;

    Utils::Optional<Instructions> llvm_ir() const;

    bool isFullStmt() const
    { return m_full_stmt; }

    void setIsFullStmt(bool yn)
    { m_full_stmt = yn; }

    bool inMacroExpansion() const
    { return m_in_macro_expansion; }

    void setInMacroExpansion(bool yn)
    { m_in_macro_expansion = yn; }

    bool isFunctionDecl() const
    { return m_class == "Function" ||
             m_class == "CXXMethod" ||
             m_class == "CXXConstructor" ||
             m_class == "CXXConversionDecl" ||
             m_class == "CXXDestructor"; }

    bool is_field_decl() const
    { return m_field_decl; }

    std::string base_type() const
    { return m_base_type; }

    bool is_bit_field() const
    { return m_bit_field; }

    unsigned bit_field_width() const
    { return m_bit_field_width; }

    unsigned long array_length() const
    { return m_array_length; }

    clang::PresumedLoc begin_src_pos() const
    { return m_begin_loc; }

    clang::PresumedLoc end_src_pos() const
    { return m_end_loc; }

    bool is_ancestor_of(AstRef ast) const;

    AstRef counter() const
    { return m_counter; }

    AstRef parent() const
    { return m_parent; }

    AuxDBEntry &aux()
    { return m_aux; }

    std::pair<AstRef, AstRef> stmt_range() const;

    std::vector<AstRef> children() const
    { return m_children; }

    typedef std::vector<AstRef>::const_iterator child_iterator;

    child_iterator begin_children() const
    { return m_children.begin(); }

    child_iterator end_children() const
    { return m_children.end(); }

    std::vector<AstRef> successors() const
    { return m_successors; }

    std::vector<std::string> annotations() const
      { return m_annotations; }

    std::string label_name() const
      { return m_label_name; }

    bool isMemberExpr() const
      { return m_is_member_expr; }

    picojson::value toJSON(const std::set<std::string> & keys,
                           bool include_aux) const;

    Ast(clang::Stmt * _stmt,
        AstRef _counter,
        AstRef _parent,
        SyntacticContext syn_ctx,
        clang::SourceRange r,
        clang::SourceRange nr,
        clang::PresumedLoc pBegin,
        clang::PresumedLoc pEnd);

    Ast(clang::Decl * _decl,
        AstRef _counter,
        AstRef _parent,
        SyntacticContext syn_ctx,
        clang::SourceRange r,
        clang::SourceRange nr,
        clang::PresumedLoc pBegin,
        clang::PresumedLoc pEnd);

    void add_child(AstRef child)
    { m_children.push_back(child); }

    void add_successor(AstRef successor)
    { m_successors.push_back(successor); }


    void expand_to_child_ranges();

    static AstRef create(clang::Stmt * stmt, Requirements & reqs)
    { return impl_create(stmt, reqs); }

    static AstRef create(clang::Decl * decl, Requirements & reqs)
    { return impl_create(decl, reqs); }

    struct Field
    {
        virtual bool has_field(TU & tu, Ast & ast) = 0;
        virtual picojson::value to_json(TU & tu, Ast & ast) = 0;
        virtual std::vector<std::string> purpose() = 0;
    };

    static std::map<std::string, Ast::Field*> & ast_fields();


private:
    void update_range_offsets(clang::CompilerInstance * ci);

    void setFieldDeclProperties(clang::ASTContext * context,
                                clang::CompilerInstance * ci);

    static std::map<std::string, Ast::Field*> s_ast_fields;

    template <typename T>
        static AstRef impl_create(T * clang_obj, Requirements & reqs);

    clang::Stmt * m_stmt;
    clang::Decl * m_decl;
    AstRef m_counter;
    AstRef m_parent;
    std::vector<AstRef> m_children;
    std::vector<AstRef> m_successors;

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
    std::vector<std::string> m_declares;
    bool m_guard;
    std::set<Hash> m_types;
    Hash m_expr_type;
    std::set<std::string> m_includes;
    PTNode m_scope_pos;
    std::set<Macro> m_macros;
    std::set<VariableInfo> m_free_vars;
    std::set<FunctionInfo> m_free_funs;
    std::string m_opcode;
    bool m_full_stmt;
    bool m_in_macro_expansion;
    SyntacticContext m_syn_ctx;
    bool m_can_have_compilation_data;
    Replacements m_replacements;
    // Additional class-specific fields
    AuxDBEntry m_aux;

    // Properties for struct field decls
    bool m_field_decl;
    std::string m_base_type;
    bool m_bit_field;
    unsigned m_bit_field_width;
    unsigned long m_array_length;
    // Properties for member exprs
    std::string m_label_name;
    bool m_is_member_expr;

    SourceOffset m_norm_start_off, m_norm_end_off, m_start_off, m_end_off;

    std::vector<std::string> m_annotations;

};

} // namespace clang_mutate

#endif
