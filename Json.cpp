#include "Json.h"

EscapedString::EscapedString(const std::string & s)
{
  for (size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
    case '\n':
      m_esc_str.append("\\n");
      break;
    case '\"':
      m_esc_str.append("\\\"");
      break;
    case '\\':
      m_esc_str.append("\\\\");
      break;
    default:
      m_esc_str.push_back(s[i]);
    }
  }
}

picojson::value EscapedString::toJSON() const
{ return picojson::value(m_esc_str); }

void append_arrays(picojson::array & xs, const picojson::array & ys)
{
    for (picojson::array::const_iterator it = ys.begin(); it != ys.end(); ++it)
        xs.push_back(*it);
}

