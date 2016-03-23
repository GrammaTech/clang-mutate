#include "AST.h"
#include "TU.h"
#include "Rewrite.h"

#include <sstream>

using namespace clang_mutate;
using namespace clang;

AstRef AstTable::nextAstRef() const
{ return asts.size() + 1; }

template <typename T>
AstRef AstTable::impl_create(T * clang_obj, Requirements & required)
{
    AstRef ref = nextAstRef();
    AstRef parent = required.parent();
    asts.push_back(Ast(clang_obj,
                       ref,
                       required.parent(),
                       required.sourceRange(),
                       required.normalizedSourceRange(),
                       required.beginLoc(),
                       required.endLoc()));
    if (parent != NoAst)
        ast(parent).add_child(ref);

    Ast & newAst = ast(ref);
    newAst.setIncludes(required.includes());
    newAst.setMacros(required.macros());
    newAst.setTypes(required.types());
    newAst.setScopePosition(required.scopePos());
    newAst.setFreeVariables(required.variables());
    newAst.setFreeFunctions(required.functions());
    newAst.setText(required.text());

    bool is_full_stmt =
        (parent == NoAst && newAst.isStmt()) ||
        (parent != NoAst && (ast(parent).isDecl() ||
                             ast(parent).className() == "CompoundStmt"));
    newAst.setIsFullStmt(is_full_stmt);
    
    Stmt * parentStmt = parent == NoAst
       ? NULL
        : ast(parent).asStmt();
    
    if (newAst.isStmt()) {
        bool has_bytes = Utils::ShouldAssociateBytesWithStmt(
            newAst.asStmt(),
            parentStmt);
        newAst.setCanHaveAssociatedBytes(has_bytes);
    }
    else {
        newAst.setCanHaveAssociatedBytes(false);
    }
    
    return ref;
}

template AstRef AstTable::impl_create<Stmt>(Stmt * stmt, Requirements & reqs);
template AstRef AstTable::impl_create<Decl>(Decl * stmt, Requirements & reqs);

Ast& AstTable::ast(AstRef ref)
{
    assert (ref != 0 && ref <= count());
    return asts[ref - 1];
}

Ast& AstTable::operator[](AstRef ref)
{ return ast(ref); }

size_t AstTable::count() const
{ return asts.size(); }


#define SET_JSON(k, v)                                    \
    do {                                                  \
      if (keys.empty() || keys.find(k) != keys.end()) {   \
          ans[k] = to_json(v);                            \
      } \
    } while(false)

std::string Ast::srcFilename(TU & tu) const
{
    SourceManager & sm = tu.ci->getSourceManager();
    return Utils::safe_realpath(
        sm.getFileEntryForID(sm.getMainFileID())->getName());
}

bool Ast::binaryAddressRange(
    TU & tu,
    BinaryAddressMap::BeginEndAddressPair & addrRange) const
{
    if (tu.addrMap.isEmpty())
        return false;
    
    SourceManager & sm = tu.ci->getSourceManager();
    unsigned int beginSrcLine = m_begin_loc.getLine();
    unsigned int   endSrcLine = m_end_loc.getLine();
    
    return tu.addrMap.lineRangeToAddressRange(
        srcFilename(tu),
        std::make_pair(beginSrcLine, endSrcLine),
        addrRange);
}

picojson::value Ast::toJSON(
    const std::set<std::string> & keys,
    TU & tu) const
{
    std::map<std::string, picojson::value> ans;
    
    SourceManager & sm = tu.ci->getSourceManager();
    AstTable & asts = tu.astTable;
    SourceRange r = sourceRange();
    
    SET_JSON("counter", counter());
    SET_JSON("parent_counter", parent());
    SET_JSON("ast_class", className());
    
    SET_JSON("src_file_name", srcFilename(tu));

    SET_JSON("is_decl", isDecl());
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

    std::ostringstream oss;
    std::string err;
    ChainedOp op = {
        getTextAs (counter(), "$result", true),
        echoTo    (oss, "$result")
    };
    op.run(tu, err);
    SET_JSON("orig_text", oss.str());

    SET_JSON("src_text", text());
    
    BinaryAddressMap::BeginEndAddressPair addrRange;
    if (canHaveAssociatedBytes() && binaryAddressRange(tu, addrRange)) {
        SET_JSON("binary_file_path", tu.addrMap.getBinaryPath());
        SET_JSON("begin_addr", addrRange.first);
        SET_JSON("end_addr", addrRange.second);
        SET_JSON("binary_contents",
                 tu.addrMap.getBinaryContentsAsStr(
                     addrRange.first,
                     addrRange.second));
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
    , m_guard(false)
    , m_scope_pos(NoNode)
    , m_macros()
    , m_text()
    , m_free_vars()
    , m_free_funs()
    , m_opcode("")
    , m_full_stmt(false)
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
    , m_guard(false)
    , m_scope_pos(NoNode)
    , m_macros()
    , m_text()
    , m_free_vars()
    , m_free_funs()
    , m_opcode("")
    , m_full_stmt(false)
{}
