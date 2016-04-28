#ifndef CLANG_MUTATE_PARSER_H
#define CLANG_MUTATE_PARSER_H

#include "Parser/Context.h"
#include "Parser/Combinators.h"
#include "Utils.h"

// Basic parsers for IR elements (translation units, ASTs, text)
#include "AstParsers.cxx"

// Descriptions of operations (how to parse and how to construct)
#include "OpParsers.cxx"

namespace clang_mutate {

//
//  interactive_op: Parse an operation to create the rewriting action, or
//                  fail with an error message.
//                  All possible commands are registered here, via the
//                  interactive_op::registered_ops type.
//
struct interactive_op;

//
//  p_ops: attempt to parse any of the given list of operations.
//         If a parser fails while parsing the command name, go on
//         to the next operation.  If parsing succeeds, the
//         operation's make method will be called to produce a
//         rewriting action.
//
template <typename Xs> struct p_ops;
template <typename X, typename ...Ys>
struct p_ops<std::tuple<X, Ys...>>
{
    constexpr static bool is_productive = false;
    typedef RewritingOpPtr type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        auto snapshot = ctx.save();
        auto match = parse<
            sequence<typename X::command, alt<spaces, eof>>>(ctx);
        ctx.restore(snapshot);
        if (!match.ok) {
            // Command did not match, try the next one.
            return parse<p_ops<std::tuple<Ys...>>>(ctx);
        }
        // Command did match; if parsing the command fails, take the
        // failure message and append some syntax help.
        auto ans = parse<fmap<Make<X>, typename X::parser>>(ctx);
        if (!ans.ok) {
            std::ostringstream oss;
            oss << ctx.error() << std::endl
                << std::endl << "  " << X::parser::describe() << std::endl;
            for (auto & line : X::purpose())
                oss << "    " << line << std::endl;
            oss << std::endl;
            ctx.fail(oss.str());
        }
        return ans;
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << std::endl
            << "  " << X::parser::describe() << std::endl << std::endl;
        for (auto & line : X::purpose())
            oss << "    " << line << std::endl;
        oss << p_ops<std::tuple<Ys...>>::describe();
        return oss.str();
    }
};

template <>
struct p_ops<std::tuple<>>
{
    typedef RewritingOpPtr type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        ans.ok = true;
        ans.result = NULL;
        return ans;
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << std::endl
            << "  (quit|q)" << std::endl
            << std::endl
            << "    Quit clang-mutate." << std::endl;
        return oss.str();
    }
};

//
//  interactive_op implementation
//
struct interactive_op
{
    constexpr static bool is_productive = false;
    typedef std::tuple
        < get_op
        , set_op
        , clear_op
        , reset_op
        , print_op
        , preview_op
        , info_op
        , types_op
        , echo_op
        , binary_op
        , ids_op
        , annotate_op
        , number_op
        , number_full_op
        , list_op
        , cut_op
        , setrange_op
        , setfunc_op
        , insert_op
        , swap_op
        , aux_op
        , ast_op
        , json_op
        , help_fields_op
        > registered_ops;

    typedef RewritingOpPtr type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        auto ans = parse<p_ops<registered_ops>>(ctx);
        if (ans.ok && !ans.result.is_valid()) {
            // No operations matched, print help.
            std::ostringstream oss;
            oss << "Unknown command. Possible commands are:" << std::endl
                << p_ops<registered_ops>::describe();
            ctx.fail(oss.str());
            ans.ok = false;
        }
        return ans;
    }
    static std::string describe() { return "<interactive-op>"; }
};


} // end namespace clang_mutate

#endif
