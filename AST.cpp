#include "AST.h"
#include "TU.h"
#include "Rewrite.h"

#include <sstream>

using namespace clang_mutate;
using namespace clang;

template <typename T>
AstRef AstTable::impl_create(T * clang_obj, Requirements & required)
{
    AstRef ref = asts.size() + 1;
    asts.push_back(Ast(clang_obj, ref, required.parent()));
    if (required.parent() != NoAst)
        asts[required.parent()].add_child(ref);

    Ast & newAst = ast(ref);
    newAst.setIncludes(required.includes());
    newAst.setMacros(required.macros());
    newAst.setTypes(required.types());
    newAst.setScopePosition(required.scopePos());
    newAst.setFreeVariables(required.variables());
    newAst.setFreeFunctions(required.functions());
    newAst.setText(required.text());
    
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
    return realpath(
        sm.getFileEntryForID(sm.getMainFileID())->getName(),
        NULL);
}

bool Ast::binaryAddressRange(
    TU & tu,
    BinaryAddressMap::BeginEndAddressPair & addrRange) const
{
    if (tu.addrMap.isEmpty())
        return false;
    
    SourceRange r = sourceRange();
    SourceManager & sm = tu.ci->getSourceManager();
    clang::PresumedLoc beginLoc = sm.getPresumedLoc(r.getBegin());
    clang::PresumedLoc   endLoc = sm.getPresumedLoc(r.getEnd());
    unsigned int beginSrcLine = beginLoc.getLine();
    unsigned int   endSrcLine = endLoc.getLine();
    
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

    bool is_full_stmt = (parent() == NoAst && isStmt()) ||
        (parent() != NoAst && (asts[parent()].isDecl() ||
                               asts[parent()].className() == "CompoundStmt"));
    SET_JSON("full_stmt", is_full_stmt);
    
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

    SET_JSON("begin_src_line", sm.getSpellingLineNumber   (r.getBegin()));
    SET_JSON("begin_src_col" , sm.getSpellingColumnNumber (r.getBegin()));
    SET_JSON("end_src_line"  , sm.getSpellingLineNumber   (r.getEnd()  ));
    SET_JSON("end_src_col"   , sm.getSpellingColumnNumber (r.getEnd()  ));

    std::ostringstream oss;
    std::string err;
    ChainedOp op = {
        getTextAs (counter(), "$result"),
        echoTo    (oss, "$result")
    };
    op.run(tu, err);
    SET_JSON("orig_text", oss.str());

    SET_JSON("src_text", text());
    
    BinaryAddressMap::BeginEndAddressPair addrRange;
    if (binaryAddressRange(tu, addrRange)) {
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
                           
Ast::Ast(Stmt * _stmt, AstRef _counter, AstRef _parent)
    : m_stmt(_stmt)
    , m_decl(NULL)
    , m_counter(_counter)
    , m_parent(_parent)
    , m_children()
    , m_class(std::string(_stmt->getStmtClassName()))
    , m_range(_stmt->getSourceRange())
    , m_guard(false)
    , m_scope_pos(NoNode)
    , m_macros()
    , m_text()
    , m_free_vars()
    , m_free_funs()
    , m_opcode("")
{
    if (isa<BinaryOperator>(m_stmt)) {
        m_opcode = static_cast<BinaryOperator*>(m_stmt)
            ->getOpcodeStr().str();
    }
}

Ast::Ast(Decl * _decl, AstRef _counter, AstRef _parent)
    : m_stmt(NULL)
    , m_decl(_decl)
    , m_counter(_counter)
    , m_parent(_parent)
    , m_children()
    , m_class(std::string(_decl->getDeclKindName()))
    , m_range(_decl->getSourceRange())
    , m_guard(false)
    , m_scope_pos(NoNode)
    , m_macros()
    , m_text()
    , m_free_vars()
    , m_free_funs()
    , m_opcode("")
{}
