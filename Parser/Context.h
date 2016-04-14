#ifndef CLANG_MUTATE_PARSER_CONTEXT_H
#define CLANG_MUTATE_PARSER_CONTEXT_H

#include <string>
#include <iostream>

namespace parser_templates {
    
//
//  parsed<T>: the result of running a T-parser. Either nothing
//             (ok = false), or the parsed result (ok = true,
//             result populated).
//
template <typename T> struct parsed       { bool ok; T result; };
template <>           struct parsed<void> { bool ok; };

//
//  parse<T,Ctx>: produce a parsed<T> from a context of type Ctx.
//
template <typename P, typename Ctx>
parsed<typename P::type> parse(Ctx & ctx)
{
    parsed<typename P::type> ans;
    if (!ctx.ok()) {
        ans.ok = false;
        return ans;
    }
    ans = P::run(ctx);
    if (!ans.ok && ctx.ok())
        ctx.fail(P::describe());
    return ans;
}

//
//  parser_context: A minimial class to represent the current
//                  parser state.
//
class parser_context
{
public:
    parser_context(const std::string & s)
        : str(s)
        , index(0)
        , failed(false)
        , err("")
    {}

    typedef size_t snapshot_t;
    snapshot_t save() const       { return index; }
    void   restore(snapshot_t i)  { failed = false; index = i; }

    bool ok()     const { return !failed; }
    bool done()   const { return failed || at_end(); }
    bool at_end() const { return index >= str.size(); }
    
    void fail(const std::string & e)
    {
        err = e;
        failed = true;
    }

    bool get(char & c)
    {
        if (!ok() || done()) return false;
        c = str[index++];
        return true;
    }

    std::string error() const { return err; }

    void indent(std::ostream & os)
    {
        size_t i = 0;
        while (i < str.size() && i < index) {
            os << (str[i] == '\t' ? "\t" : " ");
            ++i;
        }
        os << "^";
    }
    
private:
    std::string str;
    size_t index;
    bool failed;
    std::string err;
};

} // end namespace parser_templates

#endif
