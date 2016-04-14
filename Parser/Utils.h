#ifndef CLANG_MUTATE_PARSER_UTILS
#define CLANG_MUTATE_PARSER_UTILS

#include <vector>
#include <string>
#include <utility>
#include <tuple>

namespace parser_templates {
    
//
//  assert_same:  only compile if two types are the same.
//
template <typename X, typename Y> struct assert_same;
template <typename X> struct assert_same<X, X> {};

//
//  Nil: represents the end of a type-level list
//
struct Nil { typedef Nil type; bool _; };
Nil nil;

//
//  tcons: type-level cons, dropping voids.
//
template <typename X, typename Y>
                      struct tcons  { typedef std::pair<X, Y> eval; };
template <>           struct tcons<void, void> { typedef void eval; };
template <typename X> struct tcons<   X, void> { typedef X    eval; };
template <typename Y> struct tcons<void,   Y > { typedef Y    eval; };
template <>           struct tcons<void, Nil > { typedef Nil  eval; };
template <typename X> struct tcons<   X, Nil > { typedef std::tuple<X> eval; };
template <typename X, typename ...Ys>
    struct tcons<X, std::tuple<Ys...>>
{ typedef std::tuple<X, Ys...> eval; };

template <typename ...Ys>
struct tcons<void, std::tuple<Ys...>>
{ typedef std::tuple<Ys...> eval; };

//
//  tmap<F,Xs>: Type-level map. Apply the template F<> to each additional
//              argument, collecting the results into a tcons list.
//
template <template<class> class F, typename Xs> struct tmap;

template <template<class> class F, typename X, typename ...Ys>
struct tmap<F, std::tuple<X, Ys...>>
{ typedef typename tcons<F<X>, tmap<F,std::tuple<Ys...>>>::eval eval; };

template <template<class> class F, typename Y>
struct tmap<F,std::tuple<Y>>
{ typedef typename tcons<F<Y>, Nil>::eval eval; };

//
//  pcons_<X,Y>: Helper function for pcons; cons two non-void types together.
//
template <typename X, typename Y>
    std::pair<X,Y> pcons_(X x, Y y)
{ return std::make_pair(x, y); }

template <typename X, typename ...Ys>
    std::tuple<X, Ys...> pcons_(X x, std::tuple<Ys...> ys)
{ return std::tuple_cat(std::make_tuple(x), ys); }

template <typename X>
std::tuple<X> pcons_(X x, Nil end)
{ return std::make_tuple(x); }

//
//
//  pcons: value-level cons to match tcons. Builds a tcons-list of
//         parsed<T>s, omitting the parsed<void>s.
//

template <typename X, typename Y>
parsed<typename tcons<X,Y>::eval>
    pcons(const parsed<X> & x,
          const parsed<Y> & y)
{
    parsed<typename tcons<X,Y>::eval> ans;
    ans.ok = x.ok && y.ok;
    if (ans.ok)
        ans.result = pcons_(x.result, y.result);
    return ans;
}

template <>
parsed<void> pcons(const parsed<void> & x, const parsed<void> & y)
{
    parsed<void> ans;
    ans.ok = x.ok && y.ok;
    return ans;
}

template <typename X>
parsed<X> pcons(const parsed<X> & x, const parsed<void> & y)
{
    parsed<X> ans;
    ans.ok = x.ok && y.ok;
    if (ans.ok)
        ans.result = x.result;
    return ans;
}

template <typename Y>
parsed<Y> pcons(const parsed<void> & x, const parsed<Y> & y)
{
    parsed<Y> ans;
    ans.ok = x.ok && y.ok;
    if (ans.ok)
        ans.result = y.result;
    return ans;
}

template <typename X>
parsed<typename tcons<X,Nil>::eval>
    pcons(const parsed<X> & x,
          const Nil & end)
{
    parsed<std::tuple<X>> ans;
    ans.ok = x.ok;
    ans.result = std::make_tuple(x.result);
    return ans;
}

template <>
parsed<Nil> pcons(const parsed<void> & x, const Nil & end)
{
    parsed<Nil> ans;
    ans.ok = x.ok;
    return ans;
}

//
//  vector_of: get a type to represent a vector of Ts (special cases for
//             T = void and T = char)
//
template <typename T> struct vector_of       { typedef T item_type;
                                               typedef std::vector<T> type; };
template <>           struct vector_of<void> { typedef void item_type;
                                               typedef void type; };
template <>           struct vector_of<char> { typedef char item_type;
                                               typedef std::string type; }; 
//
//  push_back: append a T onto the end of a vector_of<T>
//
template <typename X>
void push_back(parsed<typename vector_of<X>::type> & vector,
               const parsed<X> & item)
{ vector.result.push_back(item.result); }

template <typename V> void push_back(V & vector, const parsed<void> & item) {}

//
//  fmap_: Helper for fmap to execute F::apply on the parsed<Dom>,
//         when the input contains a result. This is pretty much only
//         needed in order to make special cases when the domain or
//         range type is void-ish.
//

// Generic case, call "Cod F::apply(Dom&)"
template <typename F, typename Dom, typename EDom, typename Cod, typename ECod>
struct fmap_
{
    static parsed<Cod> apply(parsed<Dom> & x)
    {
        parsed<Cod> ans;
        ans.ok = x.ok;
        if (x.ok)
            ans.result = F::apply(x.result);
        return ans;
    }
};

// Domain is effectively void, call "Cod F::apply()"
template <typename F, typename Dom, typename Cod, typename ECod>
struct fmap_<F, Dom, void, Cod, ECod>
{
    static parsed<Cod> apply(parsed<Dom> & x)
    {
        parsed<Cod> ans;
        ans.ok = x.ok;
        if (x.ok)
            ans.result = F::apply();
        return ans;
    }
};

// Range is effectively void, call "void F::apply(Dom&)"
template <typename F, typename Dom, typename EDom, typename Cod>
struct fmap_<F, Dom, EDom, Cod, void>
{
    static parsed<Cod> apply(parsed<Dom> & x)
    {
        parsed<Cod> ans;
        ans.ok = x.ok;
        if (x.ok)
            F::apply(x.result);
        return ans;
    }
};

// Both domain and range are effectively void, call "void F::apply()"
template <typename F, typename Dom, typename Cod>
struct fmap_<F, Dom, void, Cod, void>
{
    typedef assert_same<void, typename F::cod> check_cod;
    typedef assert_same<void, typename F::dom> check_dom;
    static parsed<Cod> apply(parsed<Dom> & x)
    {
        parsed<Cod> ans;
        ans.ok = x.ok;
        if (x.ok)
            F::apply();
        return ans;
    }
};

template <typename T>
struct effective_type { typedef T eval; };
template <> struct effective_type<Nil> { typedef void eval; };

//
//  fmap<F, P>: apply the function F to the parsed result of P.
//
template <typename F, typename P>
struct fmap
{
    constexpr static bool is_productive = P::is_productive;
    typedef typename F::cod type;
    typedef assert_same<typename F::dom, typename P::type> is_mappable;
    template <typename Ctx> static parsed<type> run(Ctx & ctx)
    {
        auto x = parse<P>(ctx);
        parsed<type> ans = fmap_< F,
            typename F::dom, typename effective_type<typename F::dom>::eval,
            typename F::cod, typename effective_type<typename F::cod>::eval
          >::apply(x);
        return ans;
    }
    static std::string describe() { return P::describe(); }
};

//
//  Cons<T>: cons operation for a vector_of<T>
//
template <typename T>
struct Cons
{
    typedef typename
       tcons<T, typename vector_of<T>::type>::eval dom;
    typedef typename vector_of<T>::type cod;
    static cod apply(dom x) {
        cod ans = x.second;
        ans.insert(ans.begin(), x.first);
        return ans;
    }
};

template <>
struct Cons<void>
{ typedef void dom; typedef void cod; static cod apply(dom) { } };

//
// detupled: simplify a tuple, in these cases:
//             std::tuple<X>   ->  X
//             std::tuple<X,Y> ->  std::pair<X,Y>
template <typename T>
struct detuple
{
    typedef T dom; typedef T cod;
    static cod apply(dom & x) { return x; }
};

template <>
struct detuple<Nil>
{ typedef Nil dom; typedef Nil cod; static void apply() { } };

template <>
struct detuple<void>
{ typedef void dom; typedef void cod; static void apply() { } };

template <typename X>
struct detuple<std::tuple<X>>
{
    typedef std::tuple<X> dom; typedef X cod;
    static cod apply(dom & x) { return std::get<0>(x); }
};

template <typename X, typename Y>
struct detuple<std::tuple<X,Y>>
{
    typedef std::tuple<X,Y> dom; typedef std::pair<X,Y> cod;
    static cod apply(dom & x)
    { return std::make_pair(std::get<0>(x), std::get<1>(x)); }
};

//
//  Optional<T>: a value of type T, or nothing at all.
//
template <typename T>
struct Optional
{
public:
    Optional() : has_value(false), value() {}
    Optional(const T & t) : has_value(true), value(t) {}
    Optional(const Optional & x)
      : has_value(x.has_value)
      , value(x.value)
    {}

    bool get(T & x) const
    {
        if (has_value)
            x = value;
        return has_value;
    }
private:
    bool has_value;
    T value;
};

} // end namespace parser_templates

#endif
