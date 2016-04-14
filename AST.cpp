#include "AST.h"
#include "TU.h"
#include "Rewrite.h"

#include <sstream>

using namespace clang_mutate;
using namespace clang;

const AstRef clang_mutate::NoAst;

TU & AstRef::tu() const
{ return *TUs[m_tu]; }

Ast * AstRef::operator->() const
{ return TUs[m_tu]->asts[m_index - 1]; }

Ast & AstRef::operator*() const
{ return *TUs[m_tu]->asts[m_index - 1]; }

std::string AstRef::to_string() const
{
    std::ostringstream oss;
    oss << m_tu << "." << m_index;
    return oss.str();
}

template <typename T>
AstRef Ast::impl_create(T * clang_obj, Requirements & required)
{
    TU & tu = *TUs[required.tu()];
    AstRef ref = tu.nextAstRef();
    AstRef parent = required.parent();
    tu.asts.push_back(new Ast(clang_obj,
                              ref,
                              required.parent(),
                              required.sourceRange(),
                              required.normalizedSourceRange(),
                              required.beginLoc(),
                              required.endLoc()));
    if (parent != NoAst)
        parent->add_child(ref);

    ref->setIncludes(required.includes());
    ref->setMacros(required.macros());
    ref->setTypes(required.types());
    ref->setScopePosition(required.scopePos());
    ref->setFreeVariables(required.variables());
    ref->setFreeFunctions(required.functions());
    ref->setReplacements(required.replacements());

    bool is_full_stmt =
        (parent == NoAst && ref->isStmt()) ||
        (parent != NoAst && (parent->isDecl() ||
                             parent->className() == "CompoundStmt"));
    ref->setIsFullStmt(is_full_stmt);

    Stmt * parentStmt = parent == NoAst
       ? NULL
        : parent->asStmt();

    if (ref->isStmt()) {
        bool has_bytes = Utils::ShouldAssociateBytesWithStmt(
            ref->asStmt(),
            parentStmt);
        ref->setCanHaveAssociatedBytes(has_bytes);
    }
    else {
        ref->setCanHaveAssociatedBytes(false);
    }

    if (ref->isDecl() && isa<FieldDecl>(ref->asDecl())) {
        ref->setFieldDeclProperties(required.astContext());
    }

    return ref;
}

template AstRef Ast::impl_create<Stmt>(Stmt * stmt, Requirements & reqs);
template AstRef Ast::impl_create<Decl>(Decl * stmt, Requirements & reqs);

#define SET_JSON(k, v)                                    \
    do {                                                  \
      if (keys.empty() || keys.find(k) != keys.end()) {   \
          ans[k] = to_json(v);                            \
      } \
    } while(false)

std::string Ast::srcFilename() const
{
    TU & tu = m_counter.tu();
    SourceManager & sm = tu.ci->getSourceManager();
    return Utils::safe_realpath(
        sm.getFileEntryForID(sm.getMainFileID())->getName());
}

bool Ast::binaryAddressRange(
    BinaryAddressMap::BeginEndAddressPair & addrRange) const
{
    TU & tu = m_counter.tu();
    if (tu.addrMap.isEmpty())
        return false;

    unsigned int beginSrcLine = m_begin_loc.getLine();
    unsigned int   endSrcLine = m_end_loc.getLine();

    return tu.addrMap.lineRangeToAddressRange(
        srcFilename(),
        std::make_pair(beginSrcLine, endSrcLine),
        addrRange);
}

void Ast::setFieldDeclProperties(ASTContext * context)
{
    FieldDecl *D = static_cast<FieldDecl *>(asDecl());

    m_field_decl = true;
    m_field_name = D->getNameAsString();

    QualType Type = D->getType();

    if (Type->isArrayType()) {
        const ConstantArrayType *AT = context->getAsConstantArrayType(Type);
        m_base_type = AT->getElementType().getAsString();
        m_array_length = AT->getSize().getLimitedValue();
    }
    else {
        m_base_type = Type.getAsString();
    }

    if (D->isBitField()) {
        m_bit_field = true;
        m_bit_field_width = D->getBitWidthValue(*context);
    }
}

picojson::value Ast::toJSON(
    const std::set<std::string> & keys) const
{
    TU & tu = m_counter.tu();
    std::map<std::string, picojson::value> ans;

    SET_JSON("counter", counter());
    SET_JSON("parent_counter", parent());
    SET_JSON("ast_class", className());

    SET_JSON("src_file_name", srcFilename());

    SET_JSON("is_decl", isDecl());

    if (declares() != "")
        SET_JSON("declares", declares());

    SET_JSON("guard_stmt", isGuard());

    SET_JSON("full_stmt", isFullStmt());

    if (className() == "CompoundStmt")
        SET_JSON("stmt_list", children());

    if (opcode() != "")
        SET_JSON("opcode", opcode());

    SET_JSON("macros", macros());
    SET_JSON("includes", includes());
    SET_JSON("types", types());
    SET_JSON("unbound_vals", freeVariables());
    SET_JSON("unbound_funs", freeFunctions());

    SET_JSON("scopes", tu.scopes.get_names_in_scope_from(scopePosition(),
                                                         1000));

    SET_JSON("begin_src_line", m_begin_loc.getLine());
    SET_JSON("begin_src_col" , m_begin_loc.getColumn());
    SET_JSON("end_src_line"  , m_end_loc.getLine());
    SET_JSON("end_src_col"   , m_end_loc.getColumn());

    // Get the original, normalized source text.
    {
        RewriterState state;
        getTextAs(counter(), "$result", true)->run(state);
        SET_JSON("orig_text", state.vars["$result"]);
    }

    // Get the un-normalized source text, with free
    // variables and functions renamed via x -> (|x|)
    {
        RewriterState state;
        getTextAs(counter(), "$result")->run(state);
        SET_JSON("src_text", m_replacements.apply_to(state.vars["$result"]));
    }

    BinaryAddressMap::BeginEndAddressPair addrRange;
    if (canHaveAssociatedBytes() && binaryAddressRange(addrRange)) {
        SET_JSON("binary_file_path", tu.addrMap.getBinaryPath());
        SET_JSON("begin_addr", addrRange.first);
        SET_JSON("end_addr", addrRange.second);
        SET_JSON("binary_contents",
                 tu.addrMap.getBinaryContentsAsStr(
                     addrRange.first,
                     addrRange.second));
    }

    if (m_field_decl) {
        SET_JSON("field_name", m_field_name);
        SET_JSON("base_type", m_base_type);
        if (m_bit_field)
            SET_JSON("bit_field_width", m_bit_field_width);
        if (m_array_length > 1)
            SET_JSON("array_length", m_array_length);
    }

    return to_json(ans);
}

Ast::Ast(Stmt * _stmt,
         AstRef _counter,
         AstRef _parent,
         SourceRange r,
         SourceRange nr,
         PresumedLoc pBegin,
         PresumedLoc pEnd)
    : m_stmt(_stmt)
    , m_decl(NULL)
    , m_counter(_counter)
    , m_parent(_parent)
    , m_children()
    , m_class(std::string(_stmt->getStmtClassName()))
    , m_range(r)
    , m_normalized_range(nr)
    , m_begin_loc(pBegin)
    , m_end_loc(pEnd)
    , m_declares("")
    , m_guard(false)
    , m_scope_pos(NoNode)
    , m_macros()
    , m_free_vars()
    , m_free_funs()
    , m_opcode("")
    , m_full_stmt(false)
    , m_can_have_bytes(false)
    , m_replacements()
    , m_field_decl(false)
    , m_field_name()
    , m_base_type()
    , m_bit_field(false)
    , m_bit_field_width(0)
    , m_array_length(0)
{
    if (isa<BinaryOperator>(m_stmt)) {
        m_opcode = static_cast<BinaryOperator*>(m_stmt)
            ->getOpcodeStr().str();
    }
}

Ast::Ast(Decl * _decl,
         AstRef _counter,
         AstRef _parent,
         SourceRange r,
         SourceRange nr,
         PresumedLoc pBegin,
         PresumedLoc pEnd)
    : m_stmt(NULL)
    , m_decl(_decl)
    , m_counter(_counter)
    , m_parent(_parent)
    , m_children()
    , m_class(std::string(_decl->getDeclKindName()))
    , m_range(r)
    , m_normalized_range(nr)
    , m_begin_loc(pBegin)
    , m_end_loc(pEnd)
    , m_declares("")
    , m_guard(false)
    , m_scope_pos(NoNode)
    , m_macros()
    , m_free_vars()
    , m_free_funs()
    , m_opcode("")
    , m_full_stmt(false)
    , m_can_have_bytes(false)
    , m_replacements()
    , m_field_decl(false)
    , m_field_name()
    , m_base_type()
    , m_bit_field(false)
    , m_bit_field_width(0)
    , m_array_length(0)
{}
