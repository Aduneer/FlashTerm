#include "text.h"

#include <wchar.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <numeric>
#include <sstream>

namespace FlashTerm {
namespace {
// A UTF-8 continuation byte is 10xxxxxx; every other byte starts a code point.
bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

// Byte length of the code point starting at `index`.
std::size_t char_bytes(const std::string& str, std::size_t index) {
  std::size_t length = 1;
  while (index + length < str.size() &&
         is_continuation(static_cast<unsigned char>(str[index + length]))) {
    ++length;
  }
  return length;
}

// Columns occupied by one code point: 2 for CJK and other wide glyphs, 0 for
// combining marks. Falls back to 1 when the locale cannot decode the text.
std::size_t char_width(const std::string& str, std::size_t index,
                       std::size_t bytes) {
  std::mbstate_t state{};
  wchar_t wide = 0;
  const std::size_t consumed = std::mbrtowc(&wide, str.data() + index, bytes,
                                            &state);
  if (consumed == static_cast<std::size_t>(-1) ||
      consumed == static_cast<std::size_t>(-2)) {
    return 1;
  }
  const int width = ::wcwidth(wide);
  return (width < 0) ? 1 : static_cast<std::size_t>(width);
}
}  // namespace

std::string trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

std::string to_lowercase(const std::string& str) {
  std::string temp = str;
  for (char& c : temp) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return temp;
}

std::string normalize_answer(const std::string& str) {
  std::string temp = to_lowercase(str);
  temp.erase(std::remove_if(temp.begin(), temp.end(),
                            [](unsigned char ch) { return std::isspace(ch); }),
             temp.end());
  return temp;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
  std::vector<std::string> parts;
  if (str.empty()) {
    return parts;
  }
  std::stringstream ss(str);
  std::string part;
  while (std::getline(ss, part, delimiter)) {
    parts.push_back(trim(part));
  }
  return parts;
}

int levenshtein_distance(const std::string& s1, const std::string& s2) {
  const size_t len1 = s1.size();
  const size_t len2 = s2.size();

  // Only the previous row is ever read, so two rows suffice.
  std::vector<int> prev(len2 + 1);
  std::vector<int> curr(len2 + 1);
  std::iota(prev.begin(), prev.end(), 0);

  for (size_t i = 1; i <= len1; ++i) {
    curr[0] = static_cast<int>(i);
    for (size_t j = 1; j <= len2; ++j) {
      int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
      curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
    }
    prev.swap(curr);
  }
  return prev[len2];
}

std::string escape_csv_field(const std::string& field) {
  bool needs_quotes = field.find_first_of(",\"\n\r") != std::string::npos;
  if (!needs_quotes) {
    return field;
  }
  std::string escaped = "\"";
  for (char c : field) {
    if (c == '"') {
      escaped += "\"\"";
    } else {
      escaped += c;
    }
  }
  escaped += "\"";
  return escaped;
}

std::vector<std::string> parse_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string current_field;
  bool in_quotes = false;
  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];
    if (c == '"') {
      if (in_quotes && i + 1 < line.length() && line[i + 1] == '"') {
        current_field += '"';
        ++i;  // skip the escaped quote
      } else {
        in_quotes = !in_quotes;
      }
    } else if (c == ',' && !in_quotes) {
      fields.push_back(current_field);
      current_field.clear();
    } else {
      current_field += c;
    }
  }
  fields.push_back(current_field);
  return fields;
}

std::size_t display_width(const std::string& str) {
  std::size_t width = 0;
  for (std::size_t i = 0; i < str.size();) {
    const std::size_t bytes = char_bytes(str, i);
    width += char_width(str, i, bytes);
    i += bytes;
  }
  return width;
}

std::string truncate(const std::string& str, std::size_t max_width) {
  if (display_width(str) <= max_width) {
    return str;
  }
  if (max_width <= 3) {
    return std::string(max_width, '.');
  }

  const std::size_t keep = max_width - 3;
  std::size_t width = 0;
  std::size_t i = 0;
  while (i < str.size()) {
    // Whole code points only, so a multi-byte character is never cut in half.
    const std::size_t bytes = char_bytes(str, i);
    const std::size_t next = width + char_width(str, i, bytes);
    if (next > keep) break;
    width = next;
    i += bytes;
  }
  return str.substr(0, i) + "...";
}

std::string pad_right(const std::string& str, std::size_t width) {
  const std::size_t actual = display_width(str);
  if (actual >= width) {
    return str;
  }
  return str + std::string(width - actual, ' ');
}
}  // namespace FlashTerm
