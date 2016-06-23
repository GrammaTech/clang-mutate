#include "Ast.h"
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
                              required.syn_ctx(),
                              required.sourceRange(),
                              required.normalizedSourceRange(),
                              required.beginLoc(),
                              required.endLoc()));
    ref->update_range_offsets(required.CI());

    if (parent != NoAst)
        parent->add_child(ref);

    ref->setIncludes(required.includes());
    ref->setMacros(required.macros());
    ref->setTypes(required.types());
    ref->setScopePosition(required.scopePos());
    ref->setFreeVariables(required.variables());
    ref->setFreeFunctions(required.functions());
    ref->setReplacements(required.replacements());

    ref->setIsFullStmt(Utils::is_full_stmt(clang_obj, parent));

    Stmt * parentStmt = parent == NoAst
       ? NULL
        : parent->asStmt(*required.CI());

    if (ref->isStmt()) {
        bool has_bytes = Utils::ShouldAssociateBytesWithStmt(
            ref->asStmt(*required.CI()),
            parentStmt);
        ref->setCanHaveAssociatedBytes(has_bytes);
    }
    else {
        ref->setCanHaveAssociatedBytes(false);
    }

    if (ref->isDecl() && isa<FieldDecl>(ref->asDecl(*required.CI()))) {
        ref->setFieldDeclProperties(required.astContext(),
                                    required.CI());
    }

    return ref;
}

template AstRef Ast::impl_create<Stmt>(Stmt * stmt, Requirements & reqs);
template AstRef Ast::impl_create<Decl>(Decl * stmt, Requirements & reqs);

std::string Ast::srcFilename() const
{ return m_counter.tu().filename; }

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

void Ast::setFieldDeclProperties(ASTContext * context,
                                 CompilerInstance * ci)
{
    FieldDecl *D = static_cast<FieldDecl *>(asDecl(*ci));

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

    picojson::value aux = to_json(m_aux);
    assert(aux.is<picojson::object>());
    for (auto & field : aux.get<picojson::object>()) {
        ans[field.first] = field.second;
    }
    return to_json(ans);
}

bool Ast::has_bytes() const
{
    BinaryAddressMap::BeginEndAddressPair _;
    return canHaveAssociatedBytes()
        && binaryAddressRange(_);
}

SourceLocation findEndOfToken(CompilerInstance * ci, SourceLocation loc,
                              AstRef parent)
{
    SourceManager & sm = ci->getSourceManager();

    SourceLocation end_of_token =
        Lexer::getLocForEndOfToken(loc, 0, sm, ci->getLangOpts());

    // This is tricky, due to macros.

    // If the AST is within a macro expansion, then the above call may return
    // an invalid location. In that case, we can use try to use the range of
    // the parent. Often the top-level AST in a macro expansion will end at a
    // location outside the macro (e.g. if there's a semicolon afteward), in
    // which case all is well.

    // But when that's not the case, we eventually reach a parent that isn't
    // part of the macro expansion. Its range may extend too far, so we don't
    // want to use that. Instead, call getFileLoc() to translate our macro
    // location to a file location, and then get the end of the token. This
    // should always give us a valid location.
    if (end_of_token.isInvalid() && !parent->inMacroExpansion())
        return Lexer::getLocForEndOfToken(sm.getFileLoc(loc),
                                          0, sm, ci->getLangOpts());
    else
        return end_of_token;
}

void Ast::update_range_offsets(CompilerInstance * ci)
{
    SourceManager & sm = ci->getSourceManager();
    FileID mainFileID = sm.getMainFileID();

    std::pair<FileID, unsigned> decomp;

    // Apparently clang reports different source ranges for global Var
    // declarations when compared to local Var decls. This is supposed
    // to correct for the difference.
    int endShift = parent() == NoAst ? 0 : 1;

    decomp = sm.getDecomposedExpansionLoc(m_range.getBegin());
    m_start_off
        = decomp.first == mainFileID ? decomp.second
        : parent() == NoAst          ? BadOffset
                                     : parent()->initial_offset();
    decomp = sm.getDecomposedExpansionLoc(
            findEndOfToken(ci, m_range.getEnd(), parent()));
    m_end_off
        = decomp.first == mainFileID ? decomp.second - endShift
        : parent() == NoAst          ? BadOffset
                                     : parent()->final_offset();

    decomp = sm.getDecomposedExpansionLoc(m_normalized_range.getBegin());
    m_norm_start_off
        = decomp.first == mainFileID ? decomp.second
        : parent() == NoAst          ? BadOffset
                                     : parent()->initial_normalized_offset();
    decomp = sm.getDecomposedExpansionLoc(
            findEndOfToken(ci, m_normalized_range.getEnd(), parent()));
    m_norm_end_off
        = decomp.first == mainFileID ? decomp.second - endShift
        : parent() == NoAst          ? BadOffset
                                     : parent()->final_normalized_offset();

    // Make any required adjustments to the AST offsets.

    // If this is a Var and our most recent sibling is too, make sure that
    // our ranges don't overlap. If they do, cut this one short.
    if (className() == "Var") {
        TU & tu = counter().tu();
        AstRef prev = NoAst;
        if (parent() == NoAst && tu.asts.size() >= 2) {
            prev = tu.asts[tu.asts.size() - 2]->counter();
            while (prev->parent() != NoAst)
                prev = prev->parent();
            if (prev->className() != "Var")
                prev = NoAst;
        }
        else if (parent() != NoAst &&
                 parent()->className() == "DeclStmt" &&
                 !parent()->children().empty())
        {
            prev = parent()->children().back();
        }
        if (prev != NoAst) {
            prev->setSyntacticContext(SyntacticContext::ListElt());
            setSyntacticContext(SyntacticContext::FinalListElt());
        }
        if (prev != NoAst &&
            initial_offset() <= prev->final_normalized_offset())
        {
            // Scan over whitespace, and optionally a comma and some more
            // whitespace.
            // TODO: handle comments etc. This should use clang's parsing,
            //       but there will still be corner cases to handle
            //       (e.g. the , comes from an expanded macro)
            SourceOffset offset = prev->final_normalized_offset() + 1;
            std::string buf = sm.getBufferData(sm.getMainFileID()).str();
            while (isspace(buf[offset]) && offset < m_end_off) {
                ++offset;
            }
            if (buf[offset] == ',') {
                // If we do hit a comma, extend the previous Var's
                // normalized range to include it.
                prev->m_norm_end_off = offset;
                ++offset;
                while (isspace(buf[offset]) &&
                       offset < m_end_off &&
                       offset < (SourceOffset) buf.size())
                {
                    ++offset;
                }
            }
            if (offset >= m_end_off)
                offset = prev->final_normalized_offset() + 1;
            m_start_off = m_norm_start_off = offset;
        }
    }
    if (className() == "ParmVar" &&
        !parent()->children().empty() &&
        parent()->children().back()->className() == "ParmVar")
    {
        // We are a parameter, but not the first one. Extend our sibling's
        // normalized range forward until it reaches a comma.
        AstRef prev = parent()->children().back();
        prev->setSyntacticContext(SyntacticContext::ListElt());
        setSyntacticContext(SyntacticContext::FinalListElt());
        SourceOffset offset = prev->final_normalized_offset();
        std::string buf = sm.getBufferData(sm.getMainFileID()).str();
        while (buf[offset] != ',' && offset < (SourceOffset) buf.size())
            ++offset;
        prev->m_norm_end_off = offset;
    }
}

Ast::Ast(Stmt * _stmt,
         AstRef _counter,
         AstRef _parent,
         SyntacticContext syn_ctx,
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
    , m_in_macro_expansion(false)
    , m_syn_ctx(syn_ctx)
    , m_can_have_bytes(false)
    , m_replacements()
    , m_field_decl(false)
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
         SyntacticContext syn_ctx,
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
    , m_in_macro_expansion(false)
    , m_syn_ctx(syn_ctx)
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
}
