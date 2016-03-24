#ifndef CLANG_MUTATE_REWRITER_H
#define CLANG_MUTATE_REWRITER_H

#include "TU.h"

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
RewritingOpPtr setText      (AstRef ast, const std::string & text);
RewritingOpPtr setRangeText (Ast & ast1, Ast & ast2, const std::string & text);
RewritingOpPtr setRangeText (clang::SourceRange r, const std::string & text);
RewritingOpPtr insertBefore (AstRef ast, const std::string & text);
RewritingOpPtr getTextAs    (AstRef ast,
                             const std::string & var,
                             bool normalized = false);
RewritingOpPtr echoTo          (std::ostream & os, const std::string & text);
RewritingOpPtr printModifiedTo (std::ostream & os);
RewritingOpPtr printOriginalTo (std::ostream & os);
RewritingOpPtr annotateWith (Annotator * ann);
RewritingOpPtr chain (const std::vector<RewritingOpPtr> & ops);

typedef std::map<std::string, std::string> NamedText;

struct RewriterState
{
    RewriterState(clang::Rewriter & _rewriter,
                  NamedText & _vars,
                  clang::CompilerInstance * _ci,
                  AstTable & _asts)
        : rewriter(_rewriter)
        , vars(_vars)
        , asts(_asts)
        , failed(false)
        , message("")
        , ci(_ci)
    {}

    void fail(const std::string & msg);

    clang::Rewriter &  rewriter;
    NamedText & vars;
    AstTable &  asts;
    bool        failed;
    std::string message;
    clang::CompilerInstance* ci;
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
    };

    RewritingOp() : count(0) {}
    
    virtual OpKind kind() const = 0;
    virtual AstRef target() const = 0;
    virtual void print(std::ostream & o) const = 0;
    virtual void execute(RewriterState & state) const = 0;
    virtual ~RewritingOp() {}
    
    RewritingOpPtr then(RewritingOpPtr that);

    bool run(TU & tu, std::string & error_message) const;

    std::string string_value(const std::string & text,
                             AstRef tgt,
                             RewriterState & state) const;

    std::string string_value(const std::string & text,
                             RewriterState & state,
                             bool should_normalize = false) const;

    RefCounter count;
};

typedef std::vector<RewritingOpPtr> RewritingOps;

class EchoOp : public RewritingOp
{
public:
    EchoOp(const std::string & text,
           std::ostream & os)
        : RewritingOp()
        , m_text(text)
        , m_os(os)
    {}
    
    OpKind kind() const { return Op_Echo; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    std::string m_text;
    std::ostream & m_os;
};

class PrintOriginalOp : public RewritingOp
{
public:
    PrintOriginalOp(std::ostream & os) : RewritingOp(), m_os(os) {}
    OpKind kind() const { return Op_PrintOriginal; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    std::ostream & m_os;
};

class PrintModifiedOp : public RewritingOp
{
public:
    PrintModifiedOp(std::ostream & os) : RewritingOp(), m_os(os) {}
    OpKind kind() const { return Op_PrintModified; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    std::ostream & m_os;
};

class Annotator {
public:
    virtual ~Annotator() {}

    virtual std::string before(const Ast & ast);
    virtual std::string after (const Ast & ast);
    virtual std::string describe();
};

class AnnotateOp : public RewritingOp
{
public:
    AnnotateOp(Annotator * annotate) : RewritingOp(), m_annotate(annotate) {}
    OpKind kind() const { return Op_Annotate; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    Annotator * m_annotate;
};

// NOTE: ChainedOp takes ownership of it's member ops.
class ChainedOp : public RewritingOp
{
    void merge(RewritingOpPtr op);
    
public:
    ChainedOp(const RewritingOps & ops)
        : RewritingOp()
        , m_ops(1)
    { for (auto & op : ops) merge(op); }

    ChainedOp(const std::initializer_list<RewritingOpPtr> & ops)
        : RewritingOp()
        , m_ops(1)
    { for (auto & op : ops) merge(op); }

    OpKind kind() const { return Op_Chain; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:

    // An operation chain consists of an interleaved list of operations
    // of two types: mutations, which should be applied from largest AstRef
    // to smallest, and IO-style actions like printing the resulting rewrite
    // buffer or reading original source text.
    typedef std::map<AstRef, std::vector<RewritingOpPtr> > Mutations;
    typedef std::vector<RewritingOpPtr> IO;
    std::vector<std::pair<Mutations, IO> > m_ops;
};

class InsertOp : public RewritingOp
{
public:
    InsertOp(AstRef ast, const std::string & text)
        : RewritingOp()
        , m_tgt(ast)
        , m_text(text)
    {}
    
    OpKind kind() const { return Op_Insert; }
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    AstRef m_tgt;
    std::string m_text;
};

class SetOp : public RewritingOp
{
public:
    SetOp(AstRef ast, const std::string & text)
        : RewritingOp()
        , m_tgt(ast)
        , m_text(text)
    {}
    OpKind kind() const { return Op_Set; }
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    AstRef m_tgt;
    std::string m_text;
};

class SetRangeOp : public RewritingOp
{
public:
    SetRangeOp(clang::SourceRange r,
               AstRef endAst,
               const std::string & text)
        : RewritingOp()
        , m_range(r)
        , m_endAst(endAst)
        , m_text(text)
    {}
    
    OpKind kind() const { return Op_SetRange; }
    AstRef target() const { return m_endAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    clang::SourceRange m_range;
    AstRef m_endAst;
    std::string m_text;
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


std::string getAstText(
    clang::CompilerInstance * ci,
    AstTable & asts,
    AstRef ast);

} // end namespace clang_mutate

#endif
