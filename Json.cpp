#include "Json.h"

void append_arrays(picojson::array & xs, const picojson::array & ys)
{
    for (picojson::array::const_iterator it = ys.begin(); it != ys.end(); ++it)
        xs.push_back(*it);
}

