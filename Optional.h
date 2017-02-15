#ifndef OPTIONAL_H
#define OPTIONAL_H

namespace Utils {

//
//  Optional<T>: a value of type T, or nothing at all.
//
template <typename T>
struct Optional
{
public:
    Optional() : has_value(false), t() {}
    Optional(const T & t) : has_value(true), t(t) {}
    Optional(const Optional & x)
      : has_value(x.has_value)
      , t(x.t)
    {}

    operator bool() const
    {
        return has_value;
    }

    T value() const {
        return t;
    }

    bool get(T & x) const
    {
        if (has_value)
            x = t;
        return has_value;
    }

private:
    bool has_value;
    T t;
};

//
//  FromOptional<T>: Extract the value from an Optional<T> or
//                   use the default constructor for T.
//
template <typename T>
struct FromOptional
{
    typedef Optional<T> dom;
    typedef T           cod;
    static cod apply(dom x) {
        cod ans;
        if (x.get(ans))
            return ans;
        return cod();
    }
};

}

#endif
