#ifndef JSON_UTILS
#define JSON_UTILS

#include "third-party/picojson-1.3.0/picojson.h"

#include <string>

std::string unescape_from_json(const std::string &);

std::string escape_for_json(const std::string &);

void append_arrays(picojson::array & xs, const picojson::array & ys);

template <typename T>
picojson::value to_json(const T & t)
{ return picojson::value(t); }

#endif
