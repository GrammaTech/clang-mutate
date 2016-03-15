#include "Rewrite.h"
#include "Utils.h"

#include <sstream>

namespace clang_mutate {
using namespace clang;

// TODO
std::string quote_string(const std::string & s)
{ return s; }
std::string unquote_string(const std::string & s)
{ return s; }

bool valid_ast(RewriterState & state, AstRef ast, const std::string & op)
{
    if (state.failed)
        return false;
    if (ast == NoAst || ast > state.asts.count()) {
        std::ostringstream oss;
        oss << "In '" << op << "' operation, AST " << ast
            << " is outside the valid range of 1 -- "
            << state.asts.count() << ".";
        state.fail(oss.str());
        return false;
    }
    return true;
}

std::string getAstText(
    CompilerInstance * ci,
    AstTable & asts,
    AstRef ast)
{
    SourceManager & sm = ci->getSourceManager();
    SourceRange r = //normalizedSourceRange(asts[*it]); <- see Renaming.cpp:42
        Utils::getImmediateMacroArgCallerRange(
            sm, asts[ast].sourceRange());
    SourceLocation begin = r.getBegin();
    SourceLocation end = Lexer::getLocForEndOfToken(r.getEnd(),
                                                    0,
                                                    sm,
                                                    ci->getLangOpts());
    return Lexer::getSourceText(
        CharSourceRange::getCharRange(begin, end),
        sm,
        ci->getLangOpts());
}

void RewriterState::fail(const std::string & msg)
{
    failed = true;
    message = msg;
}

RewritingOp * RewritingOp::then(const RewritingOp* that) const
{
    std::vector<const RewritingOp*> these;
    these.push_back(this);
    these.push_back(that);
    return new ChainedOp(these);
}

RewritingOp * getTextAs(AstRef ast, const std::string & var)
{ return new GetOp(ast, var); }

RewritingOp * setText(AstRef ast, const std::string & text)
{ return new SetOp(ast, text); }

RewritingOp * setRangeText (AstRef ast1, AstRef ast2, const std::string & text)
{ return new SetRangeOp(ast1, ast2, text); }

RewritingOp * insertBefore(AstRef ast, const std::string & text)
{ return new InsertOp(ast, text); }

RewritingOp * echoTo(std::ostream & os, const std::string & text)
{ return new EchoOp(text, os); }

RewritingOp * printOriginalTo(std::ostream & os)
{ return new PrintOriginalOp(os); }

RewritingOp * printModifiedTo(std::ostream & os)
{ return new PrintModifiedOp(os); }

RewritingOp * annotateWith(Annotator * annotator)
{ return new AnnotateOp(annotator); }
    
bool RewritingOp::run(TU & tu, std::string & error_message) const
{
    NamedText vars;
    Rewriter rewriter;
    SourceManager & sm = tu.ci->getSourceManager();
    rewriter.setSourceMgr(sm, tu.ci->getLangOpts());
    RewriterState state(rewriter, vars, tu.ci, tu.astTable);
    execute(state);
    if (state.failed) {
        error_message = state.message;
        return false;
    }
    return true;
}

std::string RewritingOp::string_value(
    const std::string & text,
    RewriterState & state) const
{
    if (state.failed || text.empty() || text[0] != '$')
        return text;
    // Look up variable binding or fail.
    auto search = state.vars.find(text);
    if (search == state.vars.end()) {
        std::ostringstream oss;
        oss << "variable " << text << " is unbound.";
        state.fail(oss.str());
        return text;
    }
    return search->second;
}

void ChainedOp::merge(const RewritingOp * op)
{
    switch (op->kind()) {
    case Op_Chain: {
        const ChainedOp * ch = static_cast<const ChainedOp*>(op);
        bool first = true;
        for (auto p : ch->m_ops) {
            if (first && m_ops.back().second.empty()) {
                for (auto kvs : p.first) {
                    for (auto v : kvs.second) { 
                        m_ops.back().first[kvs.first].push_back(v);
                    }
                }
                m_ops.back().second = p.second;
            }
            else {
                m_ops.push_back(p);
            }
            first = false;
        }
    } break;
    case Op_Set:
    case Op_Insert:
    case Op_SetRange: {
        if (!m_ops.back().second.empty()) {
            m_ops.push_back(std::make_pair(Mutations(), IO()));
        }
        m_ops.back().first[op->target()].push_back(op);
    } break;
    default: {
        m_ops.back().second.push_back(op);
    } break;
    }
}
    
void ChainedOp::print(std::ostream & o) const
{
    o << "{ ";
    std::string sep = "";
    for (auto p : m_ops) {
        for (auto kvs : p.first) {
            for (auto op : kvs.second) {
                o << sep;
                op->print(o);
                sep = "; ";
            }
        }
        for (auto op : p.second) {
            o << sep;
            op->print(o);
            sep = "; ";
        }
    }
    o << " }";
}

void ChainedOp::execute(RewriterState & state) const
{
    if (state.failed) return;
    for (auto p : m_ops) {
        // Execute some mutations in reverse-AstRef order
        for (auto it = p.first.rbegin(); it != p.first.rend(); ++it) {
            for (auto op : it->second) {
                op->execute(state);
                if (state.failed) return;
            }
        }
        // Execution some IO
        for (auto op : p.second) {
            op->execute(state);
            if (state.failed) return;
        }
    }
}

void InsertOp::print(std::ostream & o) const
{ o << "insert " << quote_string(m_text) << " before " << m_tgt; }

void InsertOp::execute(RewriterState & state) const
{
    if (!valid_ast(state, m_tgt, "insert")) return;
    SourceRange r = // normalizedSourceRange(asts[m_tgt]);
        Utils::getImmediateMacroArgCallerRange(
            state.ci->getSourceManager(),
            state.asts[m_tgt].sourceRange());
    state.rewriter.InsertTextBefore(
        r.getBegin(),
        StringRef(string_value(m_text, state)));
}

void SetOp::print(std::ostream & o) const
{ o << "set " << m_tgt << " to " << quote_string(m_text); }

void SetOp::execute(RewriterState & state) const
{
    if (!valid_ast(state, m_tgt, "set")) return;
    SourceRange r = // normalizedSourceRange(asts[m_tgt]);
        Utils::getImmediateMacroArgCallerRange(
            state.ci->getSourceManager(),
            state.asts[m_tgt].sourceRange());
    state.rewriter.ReplaceText(r, StringRef(string_value(m_text, state)));
}

void SetRangeOp::print(std::ostream & o) const
{
    o << "setrange " << m_ast1 << " -- " << m_ast2
      << " to " << quote_string(m_text);
}

void SetRangeOp::execute(RewriterState & state) const
{
    if (!valid_ast(state, m_ast1, "setrange")) return;
    if (!valid_ast(state, m_ast2, "setrange")) return;
    SourceRange r1 = // normalizedSourceRange(asts[m_tgt]);
        Utils::getImmediateMacroArgCallerRange(
            state.ci->getSourceManager(),
            state.asts[m_ast1].sourceRange());
    SourceRange r2 = // normalizedSourceRange(asts[m_tgt]);
        Utils::getImmediateMacroArgCallerRange(
            state.ci->getSourceManager(),
            state.asts[m_ast2].sourceRange());
    SourceRange r(r1.getBegin(), r2.getEnd());
    state.rewriter.ReplaceText(r, StringRef(string_value(m_text, state)));
}

void EchoOp::print(std::ostream & o) const
{ o << "echo " << quote_string(m_text); }

void EchoOp::execute(RewriterState & state) const
{
    if (state.failed) return;
    m_os << string_value(m_text, state);
}

void PrintOriginalOp::print(std::ostream & o) const
{ o << "print_original"; }

void PrintOriginalOp::execute(RewriterState & state) const
{
    if (state.failed) return;
    SourceManager & sm = state.ci->getSourceManager();
    m_os << sm.getBufferData(sm.getMainFileID()).str();
}

void PrintModifiedOp::print(std::ostream & o) const
{ o << "print_modified"; }

void PrintModifiedOp::execute(RewriterState & state) const
{
    if (state.failed) return;
    SourceManager & sm = state.ci->getSourceManager();
    FileID file = sm.getMainFileID();
    const RewriteBuffer * buf = state.rewriter.getRewriteBufferFor(file);
    m_os << std::string(buf->begin(), buf->end());
}

void AnnotateOp::print(std::ostream & o) const
{ o << "annotate(" << m_annotate->describe() << ")"; }

void AnnotateOp::execute(RewriterState & state) const
{
    if (state.failed) return;
    SourceManager & sm = state.ci->getSourceManager();
    LangOptions & langOpts = state.ci->getLangOpts();
    
    for (auto & ast : state.asts) {
        SourceRange r = // normalizedSourceRange(ast);
            Utils::getImmediateMacroArgCallerRange(
                state.ci->getSourceManager(),
                ast.sourceRange());
        SourceLocation end = r.getEnd();

        state.rewriter.InsertText(r.getBegin(),
                                  m_annotate->before(ast),
                                  false);
        
        unsigned offset = Lexer::MeasureTokenLength(end, sm, langOpts);
        state.rewriter.InsertText(end.getLocWithOffset(offset),
                                  m_annotate->after(ast),
                                  false);
    }        
}
                       
void GetOp::print(std::ostream & o) const
{ o << "get " << m_tgt << " as " << m_var; }

void GetOp::execute(RewriterState & state) const
{
    if (!valid_ast(state, m_tgt, "get")) return;
    state.vars[m_var] = getAstText(state.ci, state.asts, m_tgt);
}

} // end namespace clang_mutate
