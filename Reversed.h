#ifndef REVERSED_H
#define REVERSED_H

namespace Utils {

template <typename T>
struct Reversed
{
    typedef typename T::const_reverse_iterator const_iterator;
    const_iterator begin() const { return x.rbegin(); }
    const_iterator end()   const { return x.rend(); }
    Reversed(const T & _x) : x(_x) {}
private:
    const T & x;
};

template <typename T> Reversed<T> reversed(const T & x)
{ return Reversed<T>(x); }

}

#endif
