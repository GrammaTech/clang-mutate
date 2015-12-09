#include "Json.h"

std::string escape_for_json(const std::string & s)
{
  std::string ans;
  for (size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
    case '\n':
      ans.append("\\n");
      break;
    case '\"':
      ans.append("\\\"");
      break;
    case '\\':
      ans.append("\\\\");
      break;
    default:
      ans.push_back(s[i]);
    }
  }
  return ans;
}

std::string unescape_from_json(const std::string & s)
{
  std::string ans;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i != s.size() - 1) {
      char c = s[++i];
      if (c == 'n')
        ans.push_back('\n');
      else
        ans.push_back(c);
      continue;
    }
    ans.push_back(s[i]);
  }
  return ans;
}

