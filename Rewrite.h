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
RewritingOpPtr setRangeText (AstRef ast1, AstRef ast2, const std::string & text);
RewritingOpPtr insertBefore (AstRef ast, const std::string & text);
RewritingOpPtr getTextAs    (AstRef ast, const std::string & var );
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

    RefCounter count;
};

typedef std::vector<RewritingOpPtr> RewritingOps;

class EchoOp : public RewritingOp
{
public:
    EchoOp(const std::string & text,
           std::ostream & os)
        : m_text(text)
        , m_os(os)
        , RewritingOp()
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
    PrintOriginalOp(std::ostream & os) : m_os(os), RewritingOp() {}
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
    PrintModifiedOp(std::ostream & os) : m_os(os), RewritingOp() {}
    OpKind kind() const { return Op_PrintModified; }
    AstRef target() const { return NoAst; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;
private:
    std::ostream & m_os;
};

class Annotator {
public:
    virtual std::string before(const Ast & ast);
    virtual std::string after (const Ast & ast);
    virtual std::string describe();
};

class AnnotateOp : public RewritingOp
{
public:
    AnnotateOp(Annotator * annotate) : m_annotate(annotate), RewritingOp() {}
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
        : m_ops(1)
        , RewritingOp()
    { for (auto & op : ops) merge(op); }

    ChainedOp(const std::initializer_list<RewritingOpPtr> & ops)
        : m_ops(1)
        , RewritingOp()
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
        : m_tgt(ast)
        , m_text(text)
        , RewritingOp()
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
        : m_tgt(ast)
        , m_text(text)
        , RewritingOp()
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
    SetRangeOp(AstRef ast1, AstRef ast2, const std::string & text)
        : m_ast1(ast1)
        , m_ast2(ast2)
        , m_text(text)
        , RewritingOp()
    {
        if (m_ast1 > m_ast2) {
            AstRef t = m_ast1;
            m_ast1 = m_ast2;
            m_ast2 = t;
        }
    }
    
    OpKind kind() const { return Op_SetRange; }
    AstRef target() const { return m_ast1; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    AstRef m_ast1;
    AstRef m_ast2;
    std::string m_text;
};

class GetOp : public RewritingOp
{
public:  
    GetOp(AstRef ast, const std::string & var)
        : m_tgt(ast)
        , m_var(var)
        , RewritingOp()
    {}
    OpKind kind() const { return Op_Get; }    
    AstRef target() const { return m_tgt; }
    void print(std::ostream & o) const;
    void execute(RewriterState & state) const;

private:
    AstRef m_tgt;
    std::string m_var;
};


std::string getAstText(
    clang::CompilerInstance * ci,
    AstTable & asts,
    AstRef ast);

} // end namespace clang_mutate

#endif
