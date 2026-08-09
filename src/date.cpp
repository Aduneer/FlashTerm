#include "date.h"

#include <cstdio>
#include <ctime>

namespace FlashTerm {

// days_from_civil / civil_from_days follow Howard Hinnant's public-domain
// civil calendar algorithms: exact for any proleptic Gregorian date, and
// pure arithmetic, so no locale or timezone state is involved.
int days_from_civil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);  // [0, 399]
  const unsigned doy =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;  // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;    // [0, 146096]
  return era * 146097 + static_cast<int>(doe) - 719468;
}

void civil_from_days(int days, int* year, unsigned* month, unsigned* day) {
  days += 719468;
  const int era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe =
      (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;  // [0, 399]
  const int y = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);  // [0, 365]
  const unsigned mp = (5 * doy + 2) / 153;                       // [0, 11]
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;               // [1, 31]
  const unsigned m = mp + (mp < 10 ? 3 : -9);                    // [1, 12]

  *year = y + (m <= 2);
  *month = m;
  *day = d;
}

int today() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return days_from_civil(local.tm_year + 1900,
                         static_cast<unsigned>(local.tm_mon + 1),
                         static_cast<unsigned>(local.tm_mday));
}

std::string format_date(int days) {
  if (days == kNoDate) return "";

  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  civil_from_days(days, &year, &month, &day);

  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", year, month, day);
  return buffer;
}

int parse_date(const std::string& text) {
  if (text.size() != 10 || text[4] != '-' || text[7] != '-') return kNoDate;

  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  if (std::sscanf(text.c_str(), "%4d-%2u-%2u", &year, &month, &day) != 3) {
    return kNoDate;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31) return kNoDate;

  // Round-tripping rejects dates that do not exist, such as 2026-02-30.
  const int days = days_from_civil(year, month, day);
  return (format_date(days) == text) ? days : kNoDate;
}

std::string describe_due(int due, int today_days) {
  if (due == kNoDate) return "new";

  const int diff = due - today_days;
  if (diff == 0) return "today";
  if (diff == 1) return "tomorrow";
  if (diff < 0) {
    const int late = -diff;
    return "overdue by " + std::to_string(late) +
           (late == 1 ? " day" : " days");
  }
  return "in " + std::to_string(diff) + " days";
}

std::string describe_due_short(int due, int today_days) {
  if (due == kNoDate) return "new";
  const int diff = due - today_days;
  if (diff <= 0) return "due";
  return std::to_string(diff) + "d";
}
}  // namespace FlashTerm
