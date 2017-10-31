#include "Json.h"

void append_arrays(picojson::array & xs, const picojson::array & ys)
{
    for (picojson::array::const_iterator it = ys.begin(); it != ys.end(); ++it)
        xs.push_back(*it);
}


void serialize_as_sexpr(const picojson::value &x, std::ostream &os)
{
    if (x.is<std::string>()) {
        // JSON escaping doesn't work for Lisp (e.g. newlines), so we have to
        // do it ourselves. This code was copied from
        // picojson::value::serialize_str() and modified.
        os << '"';
        for (auto c : x.get<std::string>()) {
            switch (c) {
            case '"':
              os << "\\\"";
              break;
            case '\\':
              os << "\\\\";
              break;
            default:
              os << c;
            }
        }
        os << '"';
    }
    else if (x.is<picojson::array>()) {
        // Serialize arrays as lists
        os << '(';
        const picojson::array array = x.get<picojson::array>();
        for (auto item : array) {
            os << ' ';
            serialize_as_sexpr(item, os);
        }
        os << ')';
    }
    else if (x.is<picojson::object>()) {
        // Serialize objects as alists
        os << '(';
        const picojson::object object = x.get<picojson::object>();
        for (auto item : object) {
            // Don't bother printing (key . value) pairs when the
            // value is NIL.  NIL may be either false or an empty list.
            if (not ((item.second.is<bool>() and            // False NIL.
                      not item.second.get<bool>()) or
                     (item.second.is<picojson::array>() and // Empty list NIL.
                      item.second.get<picojson::array>().size() == 0))) {
                std::string key = item.first;
                std::replace(key.begin(), key.end(), '_', '-');

                os << "(:" << key << " . ";
                serialize_as_sexpr(item.second, os);
                os << ')';
            }
        }
        os << ')';
    }
    else if (x.is<bool>()) {
        os << (x.get<bool>() ? "T" : "NIL");
    }
    else {
        os <<  x.to_str();
    }
}
