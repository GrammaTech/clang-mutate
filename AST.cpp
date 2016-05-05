#include "AST.h"
#include "TU.h"
#include "Rewrite.h"

#include <sstream>

using namespace clang_mutate;
using namespace clang;

const AstRef clang_mutate::NoAst;
const SourceOffset clang_mutate::BadOffset = 0xBAD0FF5E;

std::map<std::string, Ast::Field*> Ast::s_ast_fields;

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

std::pair<AstRef, AstRef> Ast::stmt_range() const
{
    return std::make_pair(
        counter(),
        m_children.empty()
        ? counter()
        : m_children.back()->stmt_range().second);
}

bool Ast::is_ancestor_of(AstRef ast) const
{
    while (ast != NoAst) {
        if (ast == counter())
            return true;
        ast = ast->parent();
    }
    return false;
}

// Register structs representing all of the AST fields.
//
#define FIELD_DEF(Name, T, Descr, Predicate, Body)           \
    struct Name##_field : public Ast::Field                  \
    {                                                        \
        static std::string name() { return #Name; }          \
        virtual bool has_field(TU & tu, Ast & ast)           \
        { return Predicate; }                                \
        static T get(TU & tu, Ast & ast) Body                \
        virtual picojson::value to_json(TU & tu, Ast & ast)  \
        { return ::to_json(get(tu, ast)); }                  \
        virtual std::vector<std::string> purpose()           \
        { return Utils::split(Descr, '\n'); }                \
    };
#include "FieldDefs.cxx"

std::map<std::string, Ast::Field*> & Ast::ast_fields()
{
    if (s_ast_fields.empty()) {
        #define FIELD_DEF(Name, T, Descr, Predicate, Body) \
            s_ast_fields[Name##_field::name()] = new Name##_field();
        #include "FieldDefs.cxx"
    }
    return s_ast_fields;
}

picojson::value Ast::toJSON(
    const std::set<std::string> & keys) const
{
    TU & tu = m_counter.tu();
    Ast & ast = *m_counter;
    std::map<std::string, picojson::value> ans;

    for (auto & field : ast_fields()) {
        if (!keys.empty() && keys.find(field.first) == keys.end())
            continue;
        if (field.second->has_field(tu, ast)) {
            ans[field.first] = field.second->to_json(tu, ast);
        }
    }
    return to_json(ans);
}

bool Ast::has_bytes() const
{
    BinaryAddressMap::BeginEndAddressPair _;
    return canHaveAssociatedBytes()
        && binaryAddressRange(_);
}

void Ast::update_range_offsets()
{
    CompilerInstance * ci = counter().tu().ci;
    SourceManager & sm = ci->getSourceManager();
    FileID mainFileID = sm.getMainFileID();

    std::pair<FileID, unsigned> decomp;

    decomp = sm.getDecomposedExpansionLoc(m_range.getBegin());
    m_start_off
        = decomp.first == mainFileID ? decomp.second
        : parent() == NoAst          ? BadOffset
                                     : parent()->initial_offset();
    decomp = sm.getDecomposedExpansionLoc(
        Lexer::getLocForEndOfToken(m_range.getEnd(),
                                   0, sm, ci->getLangOpts()));
    m_end_off
        = decomp.first == mainFileID ? decomp.second - 1
        : parent() == NoAst          ? BadOffset
                                     : parent()->final_offset();

    decomp = sm.getDecomposedExpansionLoc(m_normalized_range.getBegin());
    m_norm_start_off
        = decomp.first == mainFileID ? decomp.second
        : parent() == NoAst          ? BadOffset
                                     : parent()->initial_normalized_offset();
    decomp = sm.getDecomposedExpansionLoc(
        Lexer::getLocForEndOfToken(m_normalized_range.getEnd(),
                                   0, sm, ci->getLangOpts()));
    m_norm_end_off
        = decomp.first == mainFileID ? decomp.second - 1
        : parent() == NoAst          ? BadOffset
                                     : parent()->final_normalized_offset();
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
    , m_declares()
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
    , m_base_type()
    , m_bit_field(false)
    , m_bit_field_width(0)
    , m_array_length(0)
{
    update_range_offsets();
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
    , m_declares()
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
    , m_base_type()
    , m_bit_field(false)
    , m_bit_field_width(0)
    , m_array_length(0)
{
    // clang seems to give us the wrong source ranges for FieldDecls.
    // Work around this by setting the range to the normalized range, which
    // will grab everything up to (but not including) the semicolon.
    if (isa<FieldDecl>(_decl))
        m_range = m_normalized_range;

    update_range_offsets();
}
