#pragma once
#include <limits>
#include <string>

namespace FlashTerm {
// Dates are whole days since 1970-01-01, in local time. Integer days keep the
// scheduling arithmetic exact and free of timezone and DST surprises.
constexpr int kNoDate = std::numeric_limits<int>::min();

int today();

int days_from_civil(int year, unsigned month, unsigned day);
void civil_from_days(int days, int* year, unsigned* month, unsigned* day);

// "YYYY-MM-DD", or "" for kNoDate.
std::string format_date(int days);
// kNoDate for anything that is not a real YYYY-MM-DD date.
int parse_date(const std::string& text);

// "new", "today", "tomorrow", "in 5 days", "overdue by 2 days".
std::string describe_due(int due, int today_days);
// Compact form for table columns: "new", "due", "5d".
std::string describe_due_short(int due, int today_days);
}  // namespace FlashTerm
