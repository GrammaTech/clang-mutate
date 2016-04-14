#include "AST.h"
#include "Rewrite.h"
#include "Utils.h"

#include <sstream>

namespace clang_mutate {
using namespace clang;

std::string getAstText(
    AstRef ast,
    bool normalized)
{
    TU & tu = ast.tu();
    SourceManager & sm = tu.ci->getSourceManager();
    SourceRange r = normalized
        ? ast->normalizedSourceRange()
        : ast->sourceRange();
    SourceLocation begin = r.getBegin();
    SourceLocation end = Lexer::getLocForEndOfToken(r.getEnd(),
                                                    0,
                                                    sm,
                                                    tu.ci->getLangOpts());
    return Lexer::getSourceText(
        CharSourceRange::getCharRange(begin, end),
        sm,
        tu.ci->getLangOpts());
}

void RewriterState::fail(const std::string & msg)
{
    failed = true;
    message = msg;
}

RewritingOpPtr RewritingOp::then(RewritingOpPtr that)
{ return new ChainedOp({this, that}); }

RewritingOpPtr getTextAs(AstRef ast,
                         const std::string & var,
                         bool normalized)
{ return new GetOp(ast, var, normalized); }

RewritingOpPtr setText(AstRef ast, const std::string & text)
{ return new SetOp(ast, text); }

RewritingOpPtr setRangeText (AstRef ast1, AstRef ast2, const std::string & text)
{
    return new SetRangeOp(ast2.tuid(),
                          SourceRange(ast1->normalizedSourceRange().getBegin(),
                                      ast2->normalizedSourceRange().getEnd()),
                          ast2->counter(),
                          text);
}

RewritingOpPtr setRangeText (TURef tu, SourceRange r, const std::string & text)
{ return new SetRangeOp(tu, r, NoAst, text); }

RewritingOpPtr insertBefore(AstRef ast, const std::string & text)
{ return new InsertOp(ast, text); }

RewritingOpPtr echo(const std::string & text)
{ return new EchoOp(text); }

RewritingOpPtr echoTo(const std::string & text, const std::string & var)
{ return new EchoOp(text, var); }

RewritingOpPtr printOriginal(TURef tu)
{ return new PrintOriginalOp(tu); }

RewritingOpPtr printModified(TURef tu)
{ return new PrintModifiedOp(tu); }

RewritingOpPtr annotateWith(TURef tu, Annotator * annotator)
{ return new AnnotateOp(tu, annotator); }

RewritingOpPtr chain(const std::vector<RewritingOpPtr> & ops)
{ return new ChainedOp(ops); }

RewritingOpPtr note(const std::string & text)
{ return echoTo(text, "$$"); }

RewritingOpPtr reset_buffer(TURef tu)
{
    std::vector<TURef> tus;
    tus.push_back(tu);
    return new StateManipOp(tus);
}

RewritingOpPtr reset_buffers()
{
    std::vector<TURef> tus;
    return new StateManipOp(tus);
}

RewritingOpPtr clear_var(const std::string & var)
{
    std::vector<std::string> vars;
    vars.push_back(var);
    return new StateManipOp(vars);
}

RewritingOpPtr clear_vars()
{
    std::vector<std::string> vars;
    return new StateManipOp(vars);
}

Rewriter & RewriterState::rewriter(TURef tuid)
{
    auto search = rewriters.find(tuid);
    if (search == rewriters.end()) {
        TU * tu = TUs[tuid];
        SourceManager & sm = tu->ci->getSourceManager();
        rewriters[tuid].setSourceMgr(sm, tu->ci->getLangOpts());
        return rewriters[tuid];
    }
    return search->second;
}

bool RewritingOp::run(RewriterState & state) const
{
    if (state.failed) return false;
    execute(state);
    return !state.failed;
}

std::string RewritingOp::string_value(
    const std::string & text,
    AstRef tgt,
    RewriterState & state) const
{
    return string_value(text,
                        state,
                        (tgt == NoAst || tgt->isFullStmt()));
}

std::string RewritingOp::string_value(
    const std::string & text,
    RewriterState & state,
    bool should_normalize) const
{
    if (state.failed || text.empty())
        return text;
    if (text[0] != '$') {
        return should_normalize
            ? Utils::extendTextForFullStmt(text)
            : text;
    }
    // Look up variable binding or fail.
    auto search = state.vars.find(text);
    if (search == state.vars.end()) {
        std::ostringstream oss;
        oss << "variable " << text << " is unbound.";
        state.fail(oss.str());
        return text;
    }
    return should_normalize
        ? Utils::extendTextForFullStmt(search->second)
        : search->second;
}

void ChainedOp::merge(RewritingOpPtr op)
{
    switch (op->kind()) {
    case Op_Chain: {
        ref_ptr<ChainedOp> ch = static_pointer_cast<ChainedOp>(op);
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

bool ChainedOp::edits_tu(TURef & tu) const
{
    if (m_ops.empty())
        return false;
    const std::pair<Mutations, IO> & last_ops = m_ops.back();
    const std::vector<RewritingOpPtr> & last_muts
        = last_ops.first.begin()->second;

    // If this chain ends with an I/O action or contains no mutations, then
    // this chain does not end with an edit.
    if (!last_ops.second.empty() || last_muts.empty())
        return false;

    return last_muts.back()->edits_tu(tu);
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
{ o << "insert " << Utils::escape(m_text) << " before " << m_tgt; }

bool InsertOp::edits_tu(TURef & tu) const
{ tu = m_tgt.tuid(); return true; }

void InsertOp::execute(RewriterState & state) const
{
    std::cerr << "[" << m_text << "]" << std::endl;
    SourceRange r = m_tgt->normalizedSourceRange();
    state.rewriter(m_tgt.tuid()).InsertTextBefore(
        r.getBegin(),
        StringRef(string_value(m_text, m_tgt, state)));
}

void SetOp::print(std::ostream & o) const
{ o << "set " << m_tgt << " to " << Utils::escape(m_text); }

bool SetOp::edits_tu(TURef & tu) const
{ tu = m_tgt.tuid(); return true; }

void SetOp::execute(RewriterState & state) const
{
    SourceRange r = m_tgt->normalizedSourceRange();
    state.rewriter(m_tgt.tuid())
      .ReplaceText(r, StringRef(string_value(m_text, m_tgt, state)));
}

void SetRangeOp::print(std::ostream & o) const
{
    o << "setrange {??} " << " to " << Utils::escape(m_text);
}

bool SetRangeOp::edits_tu(TURef & tu) const
{ tu = m_tu; return true; }

void SetRangeOp::execute(RewriterState & state) const
{
    state.rewriter(m_tu)
      .ReplaceText(m_range, StringRef(string_value(m_text, m_endAst, state)));
}

void EchoOp::print(std::ostream & o) const
{
    o << "echo " << Utils::escape(m_text);
    if (m_var != "")
        o << " to " << m_var;
}

void EchoOp::execute(RewriterState & state) const
{
    if (m_var == "") {
        std::cout << string_value(m_text, state);
    }
    else {
        std::ostringstream oss;
        oss << string_value(m_text, state);
        state.vars[m_var] = oss.str();
    }
    state.vars["$$"] = "";
}

void PrintOriginalOp::print(std::ostream & o) const
{ o << "print_original"; }

void PrintOriginalOp::execute(RewriterState & state) const
{
    SourceManager & sm = TUs[m_tu]->ci->getSourceManager();
    state.vars["$$"] = sm.getBufferData(sm.getMainFileID()).str();
}

void PrintModifiedOp::print(std::ostream & o) const
{ o << "print_modified"; }

void PrintModifiedOp::execute(RewriterState & state) const
{
    SourceManager & sm = TUs[m_tu]->ci->getSourceManager();
    FileID file = sm.getMainFileID();
    const RewriteBuffer * buf = state.rewriter(m_tu).getRewriteBufferFor(file);
    state.vars["$$"] = std::string(buf->begin(), buf->end());
}

void AnnotateOp::print(std::ostream & o) const
{ o << "annotate(" << m_annotate->describe() << ")"; }

bool AnnotateOp::edits_tu(TURef & tu) const
{ tu = m_tu; return true; }

void AnnotateOp::execute(RewriterState & state) const
{
    TU * tu = TUs[m_tu];
    SourceManager & sm = tu->ci->getSourceManager();
    LangOptions & langOpts = tu->ci->getLangOpts();
    Rewriter & rewriter = state.rewriter(m_tu);

    for (auto & ast : tu->asts) {
        SourceRange r = ast->normalizedSourceRange();
        SourceLocation end = r.getEnd();

        rewriter.InsertText(r.getBegin(),
                            m_annotate->before(ast),
                            false);

        unsigned offset = Lexer::MeasureTokenLength(end, sm, langOpts);
        rewriter.InsertText(end.getLocWithOffset(offset),
                            m_annotate->after(ast),
                            false);
    }
}

void GetOp::print(std::ostream & o) const
{ o << (m_normalized ? "get' " : "get ") << m_tgt << " as " << m_var; }

void GetOp::execute(RewriterState & state) const
{
    state.vars[m_var] = getAstText(m_tgt, m_normalized);
    state.vars["$$"] = state.vars[m_var];
}

void StateManipOp::print(std::ostream & o) const
{ o << "state-manip"; }

void StateManipOp::execute(RewriterState & state) const
{
    if (m_clear_vars) {
        if (m_vars.empty()) {
            state.vars.clear();
        }
        else {
            for (auto & v : m_vars) {
                state.vars.erase(v);
            }
        }
    }
    if (m_reset_rewrite) {
        if (m_tus.empty()) {
            state.rewriters.clear();
        }
        else {
            for (auto & tu : m_tus) {
                state.rewriters.erase(tu);
            }
        }
    }
}

} // end namespace clang_mutate
