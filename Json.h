#ifndef JSON_UTILS
#define JSON_UTILS

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>
#include <set>
#include <vector>
#include <sstream>

void append_arrays(picojson::array & xs, const picojson::array & ys);

//
//  Custom serialization to JSON should be done by declaring a
//  specialization to_json<T>.  Any type that specializes
//  to_json should also define a specialization describe_json<T>
//  that creates a representation of the serialized JSON object.
//  describe_json<T> is used to auto-generate human-readable
//  documentation of the JSON objects used.
//

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

// Serializing ints would be ambiguous without this specialization.
template <> inline
picojson::value to_json(const int & t)
{ return to_json(static_cast<int64_t>(t)); }

///////////////////////////////////////////////////////////////

// Generate a string describing the structure of to_json(T t)
template <typename T> struct describe_json { static std::string str(); };

template <> struct describe_json<std::string>
{ static std::string str() { return "string"; } };

template <> struct describe_json<unsigned>
{ static std::string str() { return "unsigned"; } };

template <> struct describe_json<int>
{ static std::string str() { return "signed"; } };

template <> struct describe_json<unsigned long>
{ static std::string str() { return describe_json<unsigned int>::str(); } };

template <> struct describe_json<long>
{ static std::string str() { return describe_json<int>::str(); } };

template <> struct describe_json<bool>
{ static std::string str() { return "bool"; } };

template <typename X, typename Y>
struct describe_json<std::pair<X,Y>>
{
    static std::string str()
    {
        std::ostringstream oss;
        oss << "[" << describe_json<X>::str()
            << ", " << describe_json<Y>::str() << "]";
        return oss.str();
    }
};

template <typename X>
struct describe_json<std::vector<X>>
{
    static std::string str()
    {
        std::ostringstream oss;
        oss << "[" << describe_json<X>::str() << ", ...]";
        return oss.str();
    }
};

template <typename X>
struct describe_json<std::set<X>>
{ static std::string str() { return describe_json<std::vector<X>>::str(); } };

template <typename X>
struct describe_json<std::map<std::string, X>>
{
    static std::string str()
    {
        std::ostringstream oss;
        oss << "{ \"key\": " << describe_json<X>::str() << ", ...}";
        return oss.str();
    }
};

void serialize_as_sexpr(const picojson::value &x, std::ostream &os);

#endif
