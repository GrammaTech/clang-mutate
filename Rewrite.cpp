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
    SourceOffset first = normalized
        ? ast->initial_normalized_offset()
        : ast->initial_offset();
    SourceOffset last = normalized
        ? ast->final_normalized_offset()
        : ast->final_offset();
    if (first == BadOffset || last == BadOffset) {
        std::ostringstream oss;
        oss << "/* bad offset for " << ast << " */";
        return oss.str();
    }
    return ast.tu().source.substr(first, 1 + last - first);
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

RewritingOpPtr setText(AstRef ast,
                       const std::string & text,
                       bool allow_normalization)
{ return new SetOp(ast, text, allow_normalization); }

RewritingOpPtr setRangeText (AstRef ast1, AstRef ast2,
                             const std::string & text,
                             int preAdjust, int postAdjust)
{
    return new SetRangeOp(ast1.tuid(),
                          ast1->counter(),
                          ast2->counter(),
                          text,
                          preAdjust,
                          postAdjust);
}

RewritingOpPtr insertBefore(AstRef ast,
                            const std::string & text,
                            bool allow_normalization)
{
    if (allow_normalization)
        return new InsertOp(ast, text, false);
    else
        return new LitInsertOp(ast, text, false);
}

RewritingOpPtr insertAfter(AstRef ast,
                           const std::string & text,
                           bool allow_normalization)
{
    if (allow_normalization)
        return new InsertOp(ast, text, true);
    else
        return new LitInsertOp(ast, text, true);
}

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

EditBuffer & RewriterState::rewriter(TURef tuid)
{
    auto search = rewriters.find(tuid);
    if (search == rewriters.end()) {
        rewriters.insert(std::make_pair(tuid, EditBuffer()));
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

void ChainedOp::print(std::ostream & o) const
{
    o << "{ ";
    std::string sep = "";
    for (auto p : m_ops) {
        o << sep;
        p->print(o);
        sep = ", ";
    }
    o << " }";
}

bool ChainedOp::edits_tu(TURef & tu) const
{
    if (m_ops.empty())
        return false;
    return m_ops.back()->edits_tu(tu);
}

void ChainedOp::execute(RewriterState & state) const
{
    if (state.failed) return;
    for (auto op : m_ops) {
        op->execute(state);
        if (state.failed) return;
    }
}

void InsertOp::print(std::ostream & o) const
{
    o << "insert " << Utils::escape(m_text)
      << (m_after ? " after " : " before ") << m_tgt;
}

bool InsertOp::edits_tu(TURef & tu) const
{ tu = m_tgt.tuid(); return true; }

void InsertOp::execute(RewriterState & state) const
{
    std::cerr << "InsertOp at " << m_tgt << std::endl;
    if (m_after) {
        state.rewriter(m_tgt.tuid())
            .insertAfter(m_tgt, string_value(m_text, m_tgt, state));
    }
    else {
        state.rewriter(m_tgt.tuid())
            .insertBefore(m_tgt, string_value(m_text, m_tgt, state));
    }
}


void LitInsertOp::print(std::ostream & o) const
{
    o << "insert " << Utils::escape(m_text)
      << (m_after ? " after " : " before ") << m_tgt
      << " without normalization.";
}

bool LitInsertOp::edits_tu(TURef & tu) const
{ tu = m_tgt.tuid(); return true; }

void LitInsertOp::execute(RewriterState & state) const
{
    std::cerr << "LitInsertOp" << std::endl;
    if (m_after) {
        state.rewriter(m_tgt.tuid())
            .insertAfter(m_tgt, string_value(m_text, state, false));
    }
    else {
        state.rewriter(m_tgt.tuid())
            .insertBefore(m_tgt, string_value(m_text, state, false));
    }
}

void SetOp::print(std::ostream & o) const
{ o << "set " << m_tgt << " to " << Utils::escape(m_text); }

bool SetOp::edits_tu(TURef & tu) const
{ tu = m_tgt.tuid(); return true; }

void SetOp::execute(RewriterState & state) const
{
    if (m_normalizing) {
        state.rewriter(m_tgt.tuid())
            .replaceText(m_tgt, string_value(m_text, m_tgt, state));
    }
    else {
        state.rewriter(m_tgt.tuid())
            .replaceText(m_tgt, string_value(m_text, state, false));
    }
}

void SetRangeOp::print(std::ostream & o) const
{
    o << "setrange " << m_stmt1 << ".." << m_stmt2
      << " to " << Utils::escape(m_text);
}

bool SetRangeOp::edits_tu(TURef & tu) const
{ tu = m_tu; return true; }

void SetRangeOp::execute(RewriterState & state) const
{
    state.rewriter(m_tu)
         .replaceTextRange(m_stmt1, m_stmt2,
                           string_value(m_text, m_stmt2, state),
                           m_preAdjust, m_postAdjust);
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
    //SourceManager & sm = TUs[m_tu]->ci->getSourceManager();
    //state.vars["$$"] = sm.getBufferData(sm.getMainFileID()).str();
    state.vars["$$"] = TUs[m_tu]->source;
}

void PrintModifiedOp::print(std::ostream & o) const
{ o << "print_modified"; }

void PrintModifiedOp::execute(RewriterState & state) const
{ state.vars["$$"] = state.rewriter(m_tu).preview(TUs[m_tu]->source); }

void AnnotateOp::print(std::ostream & o) const
{ o << "annotate(" << m_annotate->describe() << ")"; }

bool AnnotateOp::edits_tu(TURef & tu) const
{ tu = m_tu; return true; }

void AnnotateOp::execute(RewriterState & state) const
{
    TU * tu = TUs[m_tu];
    EditBuffer & rewriter = state.rewriter(m_tu);

    for (auto & ast : tu->asts) {
        rewriter.insertBefore(ast->counter(), m_annotate->before(ast));
        rewriter.insertAfter (ast->counter(), m_annotate->after(ast));
    }
}

void GetOp::print(std::ostream & o) const
{ o << (m_normalized ? "get " : "get' ") << m_tgt << " as " << m_var; }

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
