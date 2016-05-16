#ifndef CLANG_MUTATE_REWRITER_H
#define CLANG_MUTATE_REWRITER_H

#include "TU.h"
#include "EditBuffer.h"
#include "SyntacticContext.h"

#include "ref_ptr.hpp"

#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <string>
#include <set>
#include <map>
#include <vector>

namespace clang_mutate
{

// Basic rewriting operations.  Build your rewriting action out
// of these, sequenced by ChainedOp::then.  Or chain a bunch
// together using ChainedOp's initializer-list constructor.
class RewritingOp;
typedef ref_ptr<RewritingOp> RewritingOpPtr;
class Annotator;
RewritingOpPtr setText      (AstRef ast,
                             const std::string & text,
                             bool allow_renormalization = true);
RewritingOpPtr setRangeText (AstRef ast1, AstRef ast2,
                             const std::string & text,
                             int preAdjust = 0, int postAdjust = 0);
RewritingOpPtr insertBefore (AstRef ast,
                             const std::string & text,
                             bool allow_renormalization = true);
RewritingOpPtr insertAfter  (AstRef ast,
                             const std::string & text,
                             bool allow_renormalization = true);
RewritingOpPtr getTextAs    (AstRef ast,
                             const std::string & var,
                             bool normalized = false);
RewritingOpPtr echo          (const std::string & text);
RewritingOpPtr echoTo        (const std::string & text, const std::string & var);
RewritingOpPtr printModified (TURef tu);
RewritingOpPtr printOriginal (TURef tu);
RewritingOpPtr annotateWith  (TURef tu, Annotator * ann);
RewritingOpPtr chain (const std::vector<RewritingOpPtr> & ops);
RewritingOpPtr note  (const std::string & text);

RewritingOpPtr reset_buffer(TURef tu);
RewritingOpPtr reset_buffers();
RewritingOpPtr clear_var(const std::string & var);
RewritingOpPtr clear_vars();

typedef std::map<std::string, std::string> NamedText;

struct RewriterState
{
    RewriterState()
        : vars()
        , failed(false)
        , message("")
    { vars["$$"] = ""; }

    void fail(const std::string & msg);

    EditBuffer & rewriter(TURef tu);

    std::map<TURef, EditBuffer> rewriters;
    NamedText   vars;
    bool        failed;
    std::string message;
};

class RewritingOp
{
public:
    enum OpKind { Op_Chain
                , Op_Insert
                , Op_Set
                , Op_Get
                , Op_Echo
                , Op_PrintModified
                , Op_PrintOriginal
                , Op_SetRange
                , Op_Annotate
                , Op_StateManip
    };

    RewritingOp() : count(0) {}

    virtual OpKind kind() const = 0;
    virtual AstRef target() const = 0;
    virtual void print(std::ostream & o) const = 0;
    virtual void execute(RewriterState & state) const = 0;
    virtual bool edits_tu(TURef & tu) const { return false; }
    virtual ~RewritingOp() {}

    RewritingOpPtr then(RewritingOpPtr that);

    bool run(RewriterState & state) const;

    std::string string_value(const std::string & text,
                             AstRef tgt,
                             SyntacticContext::Where where,
                             RewriterState & state) const;

    std::string string_value(const std::string & text,
                             RewriterState & state) const;

    RefCounter count;
};

typedef std::vector<RewritingOpPtr> RewritingOps;

class EchoOp : public RewritingOp
{
public:
    EchoOp(const std::string & text)
        : RewritingOp()
        , m_text(text)
        , m_var("")
    {}

    EchoOp(const std::string & text, const std::string & var)
        : RewritingOp()
        , m_text(text)
        , m_var(var)
    {}

    OpKind kind() const { return Op_Echo; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    std::string m_text;
    std::string m_var;
};

class PrintOriginalOp : public RewritingOp
{
public:
    PrintOriginalOp(TURef tu) : RewritingOp(), m_tu(tu) {}
    OpKind kind() const { return Op_PrintOriginal; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    TURef m_tu;
};

class PrintModifiedOp : public RewritingOp
{
public:
    PrintModifiedOp(TURef tu) : RewritingOp(), m_tu(tu) {}
    OpKind kind() const { return Op_PrintModified; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    TURef m_tu;
};

class Annotator {
public:
    virtual ~Annotator() {}

    virtual std::string before(const Ast * ast);
    virtual std::string after (const Ast * ast);
    virtual std::string describe();
};

class AnnotateOp : public RewritingOp
{
public:
    AnnotateOp(TURef tu,
               Annotator * annotate)
        : RewritingOp()
        , m_tu(tu)
        , m_annotate(annotate)
    {}
    OpKind kind() const { return Op_Annotate; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
    bool edits_tu(TURef & tu) const;
private:
    TURef m_tu;
    Annotator * m_annotate;
};

class ChainedOp : public RewritingOp
{
public:
    ChainedOp(const RewritingOps & ops)
        : RewritingOp()
        , m_ops(ops)
    {}

    ChainedOp(const std::initializer_list<RewritingOpPtr> & ops)
        : RewritingOp()
        , m_ops(ops)
    {}

    OpKind kind() const { return Op_Chain; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
    bool edits_tu(TURef & tu) const;

private:
    std::vector<RewritingOpPtr> m_ops;
};

class InsertOp : public RewritingOp
{
public:
    InsertOp(AstRef ast, const std::string & text, bool after)
        : RewritingOp()
        , m_tgt(ast)
        , m_text(text)
        , m_after(after)
    {}

    OpKind kind() const { return Op_Insert; }
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
    bool edits_tu(TURef & tu) const;

private:
    AstRef m_tgt;
    std::string m_text;
    bool m_after;
};


// Non-normalizing variant of InsertOp
class LitInsertOp : public RewritingOp
{
public:
    LitInsertOp(AstRef ast, const std::string & text, bool after)
        : RewritingOp()
        , m_tgt(ast)
        , m_text(text)
        , m_after(after)
    {}

    OpKind kind() const { return Op_Insert; }
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
    bool edits_tu(TURef & tu) const;

private:
    AstRef m_tgt;
    std::string m_text;
    bool m_after;
};

class SetOp : public RewritingOp
{
public:
    SetOp(AstRef ast,
          const std::string & text,
          bool normalizing = true)
        : RewritingOp()
        , m_tgt(ast)
        , m_text(text)
        , m_normalizing(normalizing)
    {}
    OpKind kind() const { return Op_Set; }
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
    bool edits_tu(TURef & tu) const;

private:
    AstRef m_tgt;
    std::string m_text;
    bool m_normalizing;
};

class SetRangeOp : public RewritingOp
{
public:
    SetRangeOp(TURef tu,
               AstRef stmt1,
               AstRef stmt2,
               const std::string & text,
               int preAdjust,
               int postAdjust)
        : RewritingOp()
        , m_tu(tu)
        , m_stmt1(stmt1)
        , m_stmt2(stmt2)
        , m_text(text)
        , m_preAdjust(preAdjust)
        , m_postAdjust(postAdjust)
    {}

    OpKind kind() const { return Op_SetRange; }
    AstRef target() const { return m_stmt2; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
    bool edits_tu(TURef & tu) const;

private:
    TURef m_tu;
    clang::SourceRange m_range;
    AstRef m_stmt1, m_stmt2;
    std::string m_text;
    int m_preAdjust, m_postAdjust;
};

class GetOp : public RewritingOp
{
public:
    GetOp(AstRef ast, const std::string & var, bool normalized)
        : RewritingOp()
        , m_tgt(ast)
        , m_var(var)
        , m_normalized(normalized)
    {}
    OpKind kind() const { return Op_Get; }
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    AstRef m_tgt;
    std::string m_var;
    bool m_normalized;
};

class StateManipOp : public RewritingOp
{
public:
    StateManipOp(const std::vector<std::string> & vars)
        : m_clear_vars(true)
        , m_vars(vars)
        , m_reset_rewrite(false)
        , m_tus()
    {}

    StateManipOp(const std::vector<TURef> & tus)
        : m_clear_vars(false)
        , m_vars()
        , m_reset_rewrite(true)
        , m_tus(tus)
    {}

    OpKind kind() const { return Op_StateManip; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    bool m_clear_vars;
    std::vector<std::string> m_vars;
    bool m_reset_rewrite;
    std::vector<TURef> m_tus;
};

std::string getAstText(AstRef ast, bool normalized);

} // end namespace clang_mutate

#endif
