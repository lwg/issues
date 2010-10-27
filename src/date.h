// I, Howard Hinnant, hereby place this code in the public domain.
// Since tweaked by Alisdair Meredith, also in the public domain.

#ifndef DATE_H
#define DATE_H

#include <exception>
#include <istream>
#include <ostream>
#include <locale>
#include <ctime>

namespace gregorian
{

struct day;

namespace detail
{

struct spec {
    spec() noexcept : id_{id_next++} {}

    bool operator == (const spec& y) const noexcept {return id_ == y.id_;}
    bool operator != (const spec& y) const noexcept {return id_ != y.id_;}

private:
    unsigned id_;
    static unsigned id_next;

    friend class gregorian::day;
};

}  // detail

extern const detail::spec last;
extern const detail::spec first;

struct bad_date : std::exception {
   virtual const char* what() const  noexcept {return "bad_date";}
};

struct week_day;
struct date;

struct day
{
    day(int d);
    day(detail::spec s) noexcept : d_{(unsigned char)s.id_} {}

private:
    unsigned char d_;

    friend class gregorian::date;
    friend day operator*(detail::spec s, week_day wd);
    friend day operator*(int n, week_day wd);
};

struct month
{
    month(int m) noexcept : value{m} {}
    int value;
};

struct year
{
    year(int y) noexcept : value{y} {}
    int value;
};

struct week_day
{
    week_day(int d);

private:
    int d_;

    friend class gregorian::date;
    friend day operator*(detail::spec s, week_day wd);
    friend day operator*(int n, week_day wd);
};

extern const week_day sun;
extern const week_day mon;
extern const week_day tue;
extern const week_day wed;
extern const week_day thu;
extern const week_day fri;
extern const week_day sat;

extern const month jan;
extern const month feb;
extern const month mar;
extern const month apr;
extern const month may;
extern const month jun;
extern const month jul;
extern const month aug;
extern const month sep;
extern const month oct;
extern const month nov;
extern const month dec;

namespace detail
{

struct day_month_spec {
    day_month_spec(day d, month m);

private:
    day   d_;
    month m_;

    friend class gregorian::date;
};

inline
day_month_spec::day_month_spec(day d, month m)
   : d_{d}
   , m_{m}
   {
}

struct month_year_spec
{
    month_year_spec(month m, year y);

private:
    month m_;
    year  y_;

    friend class gregorian::date;
};

inline
month_year_spec::month_year_spec(month m, year y)
   : m_{m}
   , y_{y}
   {
}

}  // detail

date operator+(const date&, month);
date operator+(const date&, year);

struct date {
    date();
    date(detail::day_month_spec dm, gregorian::year y);
    date(gregorian::day d, detail::month_year_spec my);

    int day()   const noexcept {return day_ & 0x1F;}
    int month() const noexcept {return month_ & 0x0F;}
    int year()  const noexcept {return year_;}
    int day_of_week() const noexcept  {return static_cast<int>((jdate_+3) % 7);}
    bool is_leap() const noexcept;

    date& operator+=(int d);
    date& operator++() {return *this += 1;}
    date  operator++(int) {date tmp(*this); *this += 1; return tmp;}
    date& operator-=(int d) {return *this += -d;}
    date& operator--() {return *this -= 1;}
    date  operator--(int) {date tmp(*this); *this -= 1; return tmp;}
    friend date operator+(const date& x, int y) {date r(x); r += y; return r;}
    friend date operator+(int x, const date& y) {return y + x;}
    friend date operator-(const date& x, int y) {date r(x); r += -y; return r;}

    date& operator+=(gregorian::month m) {*this = *this + m; return *this;}
    date& operator-=(gregorian::month m) {return *this += gregorian::month(-m.value);}
    friend date operator+(gregorian::month m, const date& y) {return y + m;}
    friend date operator-(const date& x, gregorian::month m) {date r(x); r -= m; return r;}

    date& operator+=(gregorian::year y) {*this = *this + y; return *this;}
    date& operator-=(gregorian::year y) {return *this += gregorian::year(-y.value);}
    friend date operator+(gregorian::year y, const date& x) {return x + y;}
    friend date operator-(const date& x, gregorian::year y) {date r(x); r -= y; return r;}

    friend long operator- (const date& x, const date& y) noexcept {return (long)(x.jdate_ - y.jdate_);}
    friend bool operator==(const date& x, const date& y) noexcept {return x.jdate_ == y.jdate_;}
    friend bool operator!=(const date& x, const date& y) noexcept {return x.jdate_ != y.jdate_;}
    friend bool operator< (const date& x, const date& y) noexcept {return x.jdate_ <  y.jdate_;}
    friend bool operator<=(const date& x, const date& y) noexcept {return x.jdate_ <= y.jdate_;}
    friend bool operator> (const date& x, const date& y) noexcept {return x.jdate_ >  y.jdate_;}
    friend bool operator>=(const date& x, const date& y) noexcept {return x.jdate_ >= y.jdate_;}

private:

    date(int y, int m, int d)
        : year_{(unsigned short)y}, month_{(unsigned char)m}, day_{(unsigned char)d} {}
    void init();
    void fix_from_ymd();
    void fix_from_jdate();
    void decode(int& dow, int& n) const noexcept;
    void encode(int dow, int n) noexcept;

    friend date operator+(const date&, gregorian::month);
    friend date operator+(const date&, gregorian::year);

private:
    unsigned long jdate_;
    unsigned short year_;
    unsigned char month_;
    unsigned char day_;
    static const unsigned char lastDay_s[12];
};

auto operator*(detail::spec s, week_day wd) -> day;
auto operator*(int n, week_day wd) -> day;


inline
auto operator/(day d, month m) -> detail::day_month_spec {
   return detail::day_month_spec{d, m};
}

inline
auto operator/(month m, day d) -> detail::day_month_spec {
   return detail::day_month_spec{d, m};
}

inline
auto operator/(year y, month m) -> detail::month_year_spec {
   return detail::month_year_spec{m, y};
}

inline
auto operator/(detail::day_month_spec dm, year y) -> date {
   return date{dm, y};
}

inline
auto operator/(detail::month_year_spec my, day d) -> date {
   return date{d, my};
}


namespace detail
{

inline
auto operator/(spec d, int m) -> detail::day_month_spec {
   return day{d} / month{m};
}

inline
auto operator/(int m, spec d) -> detail::day_month_spec {
   return day{d} / month{m};
}

inline
auto operator/(detail::day_month_spec dm, int y) -> date {
   return date{dm, year{y}};
}

inline
auto operator/(detail::month_year_spec my, int d) -> date {
   return date{day{d}, my};
}

}  // detail

template<class charT, class traits>
auto operator >>(std::basic_istream<charT,traits> & is, date & item) -> std::basic_istream<charT,traits> & {
   typename std::basic_istream<charT,traits>::sentry ok(is);
   if (ok) {
      std::ios_base::iostate err = std::ios_base::goodbit;
      try {
         std::time_get<charT> const & tg = std::use_facet<std::time_get<charT> >(is.getloc());
         std::tm t;
         tg.get_date(is, 0, is, err, &t);
         if (!(err & std::ios_base::failbit)) {
            item = date(month(t.tm_mon+1) / day(t.tm_mday) / year(t.tm_year+1900));
         }
      }
      catch (...) {
         err |= std::ios_base::badbit | std::ios_base::failbit;
      }
      is.setstate(err);
   }
   return is;
}

template<class charT, class traits>
auto operator <<(std::basic_ostream<charT, traits> & os, date const & item) -> std::basic_ostream<charT, traits> & {
   typename std::basic_ostream<charT, traits>::sentry ok{os};
   if (ok) {
      bool failed;
      try {
         std::time_put<charT> const & tp = std::use_facet<std::time_put<charT> >(os.getloc());
         std::tm t;
         t.tm_mday = item.day();
         t.tm_mon = item.month() - 1;
         t.tm_year = item.year() - 1900;
         t.tm_wday = item.day_of_week();
         charT pattern[2] = {'%', 'x'};
         failed = tp.put(os, os, os.fill(), &t, pattern, pattern+2).failed();
      }
      catch (...) {
         failed = true;
      }

      if (failed) {
         os.setstate(std::ios_base::failbit | std::ios_base::badbit);
      }
   }
   return os;
}

}  // gregorian

#endif  // DATE_H
