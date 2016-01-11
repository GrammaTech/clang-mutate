#ifndef JSON_UTILS
#define JSON_UTILS

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>
#include <set>
#include <vector>
#include <string>

void append_arrays(picojson::array & xs, const picojson::array & ys);

// Default serialization: check for a picojson::value constructor
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

template <typename T>
picojson::value to_json(const std::set<T> & ts);

// Map serialization to JSON object
template <typename T>
picojson::value to_json(const std::map<std::string,T> & obj)
{
    std::map<std::string, picojson::value> ans;
    for (typename std::map<std::string,T>::const_iterator
             it = obj.begin(); it != obj.end(); ++it)
    {
        ans[it->first] = to_json(it->second);
    }
    return picojson::value(ans);
}

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

// Set serialization, by conversion to vector
template <typename T>
picojson::value to_json(const std::set<T> & ts)
{
    std::vector<T> ans;
    for (typename std::set<T>::const_iterator
             it = ts.begin(); it != ts.end(); ++it)
    {
        ans.push_back(*it);
    }
    return to_json(ans);
}

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

#endif
