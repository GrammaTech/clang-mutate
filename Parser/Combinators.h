#ifndef CLANG_MUTATE_PARSER_COMBINATORS_H
#define CLANG_MUTATE_PARSER_COMBINATORS_H

#include "Parser/Utils.h"
#include <sstream>
#include <cctype>

namespace parser_templates {
    
// succeed: succeed without producing output or consuming input.
struct succeed;

// ignored<P>: apply P but discard the result.
template <typename P> struct ignored;

// many<P>: apply P as many times as possible, collecting
//          the results into a vector_of P's result type.
template <typename P> struct many;

// alt<Xs...>: apply the parsers Xs in order; if one fails,
//              back up and try the next one.  All of the
//              parsers must parse values of the ssame type.
template <typename X, typename ...Ys> struct alt;

// alt_<Xs>: Same as alt, but operates on a tcons list directly.
template <typename Xs> struct alt_;

// sequence<Xs...>: variadic template; apply the sequence of
//                  parsers and collect the results into a
//                  tcons list.
template <typename X, typename ...Ys> struct sequence;

// sequence_<Xs...>: like sequence, but the last parser is
//                   not cons'd with nil.
template <typename X, typename ...Ys> struct sequence_;

// pair_of<X,Y>: parse an X followed by a Y, resulting in a
//               parsed std::pair<X,Y>.
template <typename X, typename Y> struct pair_of;

// sepBy<P, S>: Apply P one or more times, applying (and ignoring) S
//              in between.
template <typename P, typename S> using
    sepBy = fmap<Cons<typename P::type>,
                 pair_of<P, many<sequence_<ignored<S>, P>>>>;

// many1<P>: apply P as many times as possible (but at least 1 time),
//           collecting the results into a vector_of P's result type.
template <typename P> struct Many1 { typedef pair_of<P, many<P>> eval; };
template <typename P> using
    many1 = fmap<Cons<typename P::type>, typename Many1<P>::eval>;

//
// optional<P>: parse an Optional<T>, producing a value when P succeeds
//              or no value otherwise.
//
template <typename P> struct optional;

// whitespace: parse a whitespace character.
struct whitespace;

// non_whitespace: parse a non-whitespace character.
struct non_whitespace;

// chr<c>: parse the character 'c'.
template <char c> struct chr;

// str<s>: parse the string s.
template <char const * s> struct str;

// digit: parse a single digit
struct digit;

// number: parse as many digits as possible and combine into a number.
struct number;

// symbol: a non-whitespace character that can not appear in an identifier.
typedef alt< chr<','>, chr<':'>, chr<';'> > symbol;

// nonsymbol: a non-whitespace, non-symbol character.
struct non_symbol;

// eof: succeed if the end of the input stream has been reached.
struct eof;

//
//  matches<Ps...,Ctx>: check if the sequence of parsers will succeed
//                      but do not consume any input.
//
template <typename ...Ps, typename Ctx>
bool matches(Ctx & ctx)
{
    auto snapshot = ctx.save();
    auto ans = parse<sequence_<Ps...>>(ctx);
    ctx.restore(snapshot);
    return ans.ok;
}

// implementation of the basic combinators:
#define IN_COMBINATORS_DOT_H
#include "Combinators.cxx"
#undef  IN_COMBINATORS_DOT_H

// Some additional derived combinators:
//

// chr_<c>: parse a character but ignore the result.
template <char c> using
    chr_ = ignored<chr<c>>;

// str_: parse a string but ignore the result.
template <char const * s> using
    str_ = ignored<str<s>>;

// word: a non-empty sequence of non_symbols or non-empty sequence
//       of symbols.
typedef alt
    < many1<non_symbol>
    , many1<symbol>
    > word;

// spaces: a non-empty sequence of spaces, discarding the result.
typedef ignored<many1<whitespace>> spaces;

// try_<P>: run the parser P, discarding the result. If P failed,
//          rewind the input state to before P ran.
template <typename P> using
    try_ = alt<ignored<P>, succeed>;

// detupled<P>: apply the parser P and detuple the results.
template <typename P> using
  detupled = fmap<detuple<typename P::type>, P>;

// tokens<Ps...>: Parse leading whitespace, then alternate applying
//                each parser with whitespace in between, followed
//                by any trailing whitespace.  Detuples the result
//                type.
template <typename ... Ps> struct _tokens;

template <typename P, typename ... Ps>
struct _tokens<P, Ps...>
{ typedef sequence_< spaces, P, typename _tokens<Ps...>::parser > parser; };

// When parsing an optional<> token string, the trailing spaces should also be
// optional.
template <typename P, typename ... Ps>
struct _tokens<optional<P>, Ps...>
{
    typedef sequence_
        < optional<sequence_<spaces, P>>
        , typename _tokens<Ps...>::parser
    > parser;
};

template <typename P>
struct _tokens<optional<P>>
{ typedef sequence<optional<sequence_<spaces, P>>> parser; };

template <typename P>
struct _tokens<P>
{ typedef sequence<spaces, P> parser; };

template <>
struct _tokens<> { typedef sequence<succeed> parser; };

template <typename ...Ps> struct tokens;

template <typename P, typename ... Ps>
    struct tokens<P,Ps...>
{
    typedef detupled<sequence_<P, typename _tokens<Ps...>::parser>> toks;
    typedef typename toks::type type;
    typedef sequence_< try_<spaces>, toks, try_<spaces> > toks_w_padding;
    typedef assert_same<typename toks_w_padding::type, type> sanity_check;
    constexpr static bool is_productive = toks::is_productive;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { return parse<toks_w_padding>(ctx); }
    static std::string describe() { return toks::describe(); }
};

template <typename ...Ps>
    struct Many1<tokens<Ps...>>
{
    typedef typename tokens<Ps...>::toks toks; // the token parser, without
                                               // leading/trailing spaces
    typedef sequence_
        < try_<spaces>
        , pair_of<toks, many<sequence_<spaces, toks>>>
        , try_<spaces>
        > eval;
};
        
// Nicer pretty-printing
template <> std::string      try_<spaces>::describe() { return ""; }
template <> std::string many1<whitespace>::describe() { return " "; }
template <> std::string many1<non_whitespace>::describe() { return "<token>"; }
template <> std::string word::describe() { return "<word>"; }

} // end namespace parser_templates

#endif
