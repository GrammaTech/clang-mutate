#ifndef JSON_UTILS
#define JSON_UTILS

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>

class EscapedString
{
public:
    EscapedString(const std::string & s);
    EscapedString(const EscapedString & e)
        : m_esc_str(e.m_esc_str)
    {}
    picojson::value toJSON() const;

    template <typename T>
        static picojson::value map_to_json(const std::map<std::string, T> & obj);

private:
    std::string m_esc_str;
};

void append_arrays(picojson::array & xs, const picojson::array & ys);

template <typename T>
picojson::value to_json(const T & t)
{ return picojson::value(t); }

// Forward-declared specializations for templates
template <typename X,typename Y>
picojson::value to_json(const std::pair<X,Y> & xy);

template <typename T>
picojson::value to_json(const std::map<std::string,T> & obj);

template <typename T>
picojson::value to_json(const std::vector<T> & ts);

// Escaped string serialization
template <> inline
picojson::value to_json(const EscapedString & s)
{ return s.toJSON(); }

// String serialization, with escaping
template <> inline
picojson::value to_json(const std::string & s)
{ return to_json(EscapedString(s)); }

// Map serialization to JSON object
template <typename T>
picojson::value to_json(const std::map<std::string,T> & obj)
{ return EscapedString::map_to_json(obj); }

// Vector serialization, inductive case
template <typename T>
picojson::value to_json(const std::vector<T> & ts)
{
    std::vector<picojson::value> ans;
    for (typename std::vector<T>::const_iterator
             it = ts.begin(); it != ts.end(); ++it)
    {
        ans.push_back(to_json(*it));
    }
    return to_json(ans);
}

// Vector serialization, base case
template <> inline
picojson::value to_json(const std::vector<picojson::value> & ts)
{ return picojson::value(ts); }

// Pair serialization
template <typename X,typename Y>
picojson::value to_json(const std::pair<X,Y> & xy)
{
    std::vector<picojson::value> ans;
    ans.push_back(to_json(xy.first));
    ans.push_back(to_json(xy.second));
    return to_json(ans);
}

// unsigned ints are encoded as int64_t.
template <> inline
picojson::value to_json(const unsigned int & t)
{ return to_json(static_cast<int64_t>(t)); }

template <> inline
picojson::value to_json(const unsigned long & t)
{ return to_json(static_cast<int64_t>(t)); }

template <typename T>
picojson::value
EscapedString::map_to_json(const std::map<std::string, T> & obj)
{
    std::map<std::string, picojson::value> ans;
    for (typename std::map<std::string,T>::const_iterator
             it = obj.begin(); it != obj.end(); ++it)
    {
        EscapedString s(it->first);
        ans[s.m_esc_str] = to_json(it->second);
    }
    return picojson::value(ans);
}

#endif
