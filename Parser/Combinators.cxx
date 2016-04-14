#ifndef IN_COMBINATORS_DOT_H
#error "This file should only be #included from within Combinators.h"
#endif

struct succeed
{
    constexpr static bool is_productive = false;
    typedef void type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        ans.ok = true;
        return ans;
    }
    static std::string describe() { return ""; }
};

template <typename P>
struct ignored
{
    constexpr static bool is_productive = P::is_productive;
    typedef void type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { 
        parsed<type> ans;
        ans.ok = parse<P>(ctx).ok;
        return ans;
    }
    static std::string describe() { return P::describe(); }
};

template <typename X>
struct many
{
    constexpr static bool is_productive = X::is_productive;
    typedef typename vector_of<typename X::type>::type type;
    typedef typename vector_of<typename X::type>::item_type item_type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        parsed<typename X::type> item;
        while (true) {
            auto snapshot = ctx.save();
            item = parse<X>(ctx);
            if (!item.ok) {
                ctx.restore(snapshot);
                break;
            }
            push_back<item_type>(ans, item);
        }
        ans.ok = true;
        return ans;
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << "[" << X::describe() << "...]";
        return oss.str();
    }
};

template <typename X, typename... Ys>
struct alt
{
    constexpr static bool is_productive =
        X::is_productive && alt<Ys...>::is_productive;
    typedef typename X::type type;
    typedef assert_same<type, typename alt<Ys...>::type> _;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        auto snapshot = ctx.save();
        parsed<type> t = parse<X>(ctx);
        if (t.ok)
            return t;
        ctx.restore(snapshot);
        return parse<alt<Ys...>>(ctx);
    }
    static std::string describe_()
    {
        std::ostringstream oss;
        oss << "|" << X::describe() << alt<Ys...>::describe_();
        return oss.str();
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << "(" << X::describe() << alt<Ys...>::describe_();
        return oss.str();
    }
};

template <typename Y>
struct alt<Y>
{
    constexpr static bool is_productive = Y::is_productive;
    typedef typename Y::type type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { return parse<Y>(ctx); }
    static std::string describe() { return Y::describe(); }
    static std::string describe_()
    {
        std::ostringstream oss;
        oss << "|" << Y::describe() << ")";
        return oss.str();
    }
};

template <typename Xs> struct alt_;

template <typename X, typename ...Ys>
struct alt_<std::tuple<X, Ys...>>
{
    constexpr static bool is_productive =
        X::is_productive && alt_<Ys...>::is_productive;
    typedef alt<X, alt_<Ys...>> expanded;
    typedef typename expanded::type type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { return parse<expanded>(ctx); }
    static std::string describe() { return expanded::describe(); }
};

template <typename Y>
struct alt_<std::tuple<Y>>
{
    constexpr static bool is_productive = Y::is_productive;
    typedef typename Y::type type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { return parse<Y>(ctx); }
    static std::string describe() { return Y::describe(); }
};
    
template <typename X, typename... Ys>
struct sequence
{
    constexpr static bool is_productive =
        X::is_productive || sequence<Ys...>::is_productive;
    typedef typename tcons<typename X::type,
                           typename sequence<Ys...>::type>::eval type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<typename X::type> x
            = parse<X>(ctx);
        parsed<typename sequence<Ys...>::type> ys
            = parse<sequence<Ys...>>(ctx);
        return pcons(x, ys);
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << X::describe() << sequence<Ys...>::describe();
        return oss.str();
    }
};

template <typename Y>
struct sequence<Y>
{
    constexpr static bool is_productive = Y::is_productive;
    typedef typename tcons<typename Y::type, Nil>::eval type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { return pcons(parse<Y>(ctx), nil); }
    static std::string describe() { return Y::describe(); }
};

template <typename X, typename... Ys>
struct sequence_
{
    constexpr static bool is_productive =
        X::is_productive || sequence_<Ys...>::is_productive;
    typedef typename tcons<typename X::type,
                           typename sequence_<Ys...>::type>::eval type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<typename X::type> x
            = parse<X>(ctx);
        parsed<typename sequence_<Ys...>::type> ys
            = parse<sequence_<Ys...>>(ctx);
        return pcons(x, ys);
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << X::describe() << sequence<Ys...>::describe();
        return oss.str();
    }
};

template <typename Y>
struct sequence_<Y>
{
    constexpr static bool is_productive = Y::is_productive;
    typedef typename Y::type type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    { return parse<Y>(ctx); }
    static std::string describe() { return Y::describe(); }
};

template <typename X, typename Y>
struct pair_of
{
    constexpr static bool is_productive =
        X::is_productive || Y::is_productive;
    typedef std::pair<typename X::type, typename Y::type> type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        auto x = parse<X>(ctx);
        if (!x.ok) {
            ans.ok = false;
            return ans;
        }
        auto y = parse<Y>(ctx);
        ans.ok = x.ok && y.ok;
        ans.result = std::make_pair(x.result, y.result);
        return ans;
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << X::describe() << Y::describe();
        return oss.str();
    }
};

template <typename P>
struct optional
{
    constexpr static bool is_productive = false;
    typedef Optional<typename P::type> type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        auto snapshot = ctx.save();
        auto t = parse<P>(ctx);
        if (!t.ok) {
            ctx.restore(snapshot);
            ans.ok = true;
            ans.result = type();
            return ans;
        }
        ans.ok = true;
        ans.result = type(t.result);
        return ans;
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << "[" << P::describe() << "]";
        return oss.str();
    }
};

struct whitespace
{
    constexpr static bool is_productive = true;
    typedef char type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        if (!ctx.get(ans.result))
            ans.ok = false;
        ans.ok = isspace(ans.result);
        return ans;
    }
    static std::string describe() { return " "; }
};

struct non_whitespace
{
    constexpr static bool is_productive = true;
    typedef char type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        ans.ok = ctx.get(ans.result)
            ? !isspace(ans.result)
            : false;
        return ans;
    }
    static std::string describe() { return "<non-whitespace>"; }
};

template <char c>
struct chr
{
    constexpr static bool is_productive = true;
    typedef char type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans = non_whitespace::run(ctx);
        ans.ok = ans.ok && (ans.result == c);
        return ans;
    }
    static std::string describe()
    {
        std::ostringstream oss;
        oss << c;
        return oss.str();
    }
};

struct digit
{
    constexpr static bool is_productive = true;
    typedef size_t type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<char> c = non_whitespace::run(ctx);
        parsed<size_t> ans;
        ans.ok = c.ok && isdigit(c.result);
        ans.result = (size_t)(c.result - '0');
        return ans;
    }
    static std::string describe() { return "[0-9]"; }
};

struct number
{
    constexpr static bool is_productive = true;
    typedef size_t type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<std::vector<size_t> > digits = parse<many1<digit>>(ctx);
        parsed<size_t> ans;
        if (!digits.ok) {
            ans.ok = false;
            return ans;
        }
        ans.ok = true;
        ans.result = 0;
        for (auto & digit : digits.result)
            ans.result = digit + (10 * ans.result);
        return ans;
    }
    static std::string describe() { return "[0-9]+"; }
};

template <char const * s>
struct str
{
    typedef std::string type;
    constexpr static bool is_productive = true;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        std::string tgt(s);
        parsed<std::string> ans;
        for (auto c : tgt) {
            char ch;
            if (!ctx.get(ch) || ch != c) {
                ans.ok = false;
                return ans;
            }
            ans.result.push_back(c);
        }
        ans.ok = true;
        return ans;
    }
    static std::string describe() { return s; }
};

struct non_symbol
{
    constexpr static bool is_productive = true;
    typedef char type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        if (matches<whitespace>(ctx) || matches<symbol>(ctx)) {
            parsed<type> ans;
            ans.ok = false;
            return ans;
        }
        return parse<non_whitespace>(ctx);
    }
    static std::string describe() { return "non-symbol"; }
};

struct eof
{
    constexpr static bool is_productive = false;
    typedef void type;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        parsed<type> ans;
        ans.ok = ctx.at_end();
        return ans;
    }
    static std::string describe() { return ""; }
};


