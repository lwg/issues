// I, Howard Hinnant, hereby place this code in the public domain.

#ifndef DATE_H
#define DATE_H

#include <exception>
#include <istream>
#include <ostream>
#include <locale>
#include <ctime>

namespace gregorian
{

class day;

namespace detail
{

class spec
{
private:
    unsigned id_;
    static unsigned id_next;

    friend class gregorian::day;
public:
    spec() : id_(id_next++) {}
    bool operator == (const spec& y) const {return id_ == y.id_;}
    bool operator != (const spec& y) const {return id_ != y.id_;}
};

}  // detail

extern const detail::spec last;
extern const detail::spec first;

class bad_date
    : public std::exception
{
public:
    virtual const char* what() const throw() {return "bad_date";}
};

class week_day;
class date;

class day
{
private:
    unsigned char d_;

    friend class gregorian::date;
    friend day operator*(detail::spec s, week_day wd);
    friend day operator*(int n, week_day wd);
public:
    day(int d);
    day(detail::spec s) : d_((unsigned char)s.id_) {}
};

struct month
{
    month(int m) : value(m) {}
    int value;
};

struct year
{
    year(int y) : value(y) {}
    int value;
};

class week_day
{
private:
    int d_;

    friend class gregorian::date;
    friend day operator*(detail::spec s, week_day wd);
    friend day operator*(int n, week_day wd);
public:
    week_day(int d);
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

class day_month_spec
{
private:
    day   d_;
    month m_;

    friend class gregorian::date;
public:
    day_month_spec(day d, month m);
};

inline
day_month_spec::day_month_spec(day d, month m)
    :   d_(d),
        m_(m)
{
}

class month_year_spec
{
private:
    month m_;
    year  y_;

    friend class gregorian::date;
public:
    month_year_spec(month m, year y);
};

inline
month_year_spec::month_year_spec(month m, year y)
    :   m_(m),
        y_(y)
{
}

}  // detail

date operator+(const date&, month);
date operator+(const date&, year);

class date
{
private:
    unsigned long jdate_;
    unsigned short year_;
    unsigned char month_;
    unsigned char day_;
    static const unsigned char lastDay_s[12];

public:
    date();
    date(detail::day_month_spec dm, gregorian::year y);
    date(gregorian::day d, detail::month_year_spec my);

    int day() const {return day_ & 0x1F;}
    int month() const {return month_ & 0x0F;}
    int year() const {return year_;}
    int day_of_week() const {return static_cast<int>((jdate_+3) % 7);}
    bool is_leap() const;

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

    friend long operator-(const date& x, const date& y) {return (long)(x.jdate_ - y.jdate_);}
    friend bool operator==(const date& x, const date& y) {return x.jdate_ == y.jdate_;}
    friend bool operator!=(const date& x, const date& y) {return x.jdate_ != y.jdate_;}
    friend bool operator< (const date& x, const date& y) {return x.jdate_ <  y.jdate_;}
    friend bool operator<=(const date& x, const date& y) {return x.jdate_ <= y.jdate_;}
    friend bool operator> (const date& x, const date& y) {return x.jdate_ >  y.jdate_;}
    friend bool operator>=(const date& x, const date& y) {return x.jdate_ >= y.jdate_;}

private:

    date(int y, int m, int d)
        : year_((unsigned short)y), month_((unsigned char)m), day_((unsigned char)d) {}
    void init();
    void fix_from_ymd();
    void fix_from_jdate();
    void decode(int& dow, int& n) const;
    void encode(int dow, int n);

    friend date operator+(const date&, gregorian::month);
    friend date operator+(const date&, gregorian::year);
};

day operator*(detail::spec s, week_day wd);
day operator*(int n, week_day wd);

detail::day_month_spec
inline
operator/(day d, month m)
{
    return detail::day_month_spec(d, m);
}

inline
detail::day_month_spec
operator/(month m, day d)
{
    return detail::day_month_spec(d, m);
}

inline
detail::month_year_spec
operator/(year y, month m)
{
    return detail::month_year_spec(m, y);
}

inline
date
operator/(detail::day_month_spec dm, year y)
{
    return date(dm, y);
}

inline
date
operator/(detail::month_year_spec my, day d)
{
    return date(d, my);
}

namespace detail
{

detail::day_month_spec
inline
operator/(spec d, int m)
{
    return day(d) / month(m);
}

inline
detail::day_month_spec
operator/(int m, spec d)
{
    return day(d) / month(m);
}

inline
date
operator/(detail::day_month_spec dm, int y)
{
    return date(dm, year(y));
}

inline
date
operator/(detail::month_year_spec my, int d)
{
    return date(day(d), my);
}



}  // detail

template<class charT, class traits>
std::basic_istream<charT,traits>&
operator >>(std::basic_istream<charT,traits>& is, date& item)
{
    typename std::basic_istream<charT,traits>::sentry ok(is);
    if (ok)
    {
        std::ios_base::iostate err = std::ios_base::goodbit;
        try
        {
            const std::time_get<charT>& tg = std::use_facet<std::time_get<charT> >
                                                           (is.getloc());
            std::tm t;
            tg.get_date(is, 0, is, err, &t);
            if (!(err & std::ios_base::failbit))
                item = date(month(t.tm_mon+1) / day(t.tm_mday) / year(t.tm_year+1900));
        }
        catch (...)
        {
            err |= std::ios_base::badbit | std::ios_base::failbit;
        }
        is.setstate(err);
    }
    return is;
}

template<class charT, class traits>
std::basic_ostream<charT, traits>&
operator <<(std::basic_ostream<charT, traits>& os, const date& item)
{
    typename std::basic_ostream<charT, traits>::sentry ok(os);
    if (ok)
    {
        bool failed;
        try
        {
            const std::time_put<charT>& tp = std::use_facet<std::time_put<charT> >
                                                           (os.getloc());
            std::tm t;
            t.tm_mday = item.day();
            t.tm_mon = item.month() - 1;
            t.tm_year = item.year() - 1900;
            t.tm_wday = item.day_of_week();
            charT pattern[2] = {'%', 'x'};
            failed = tp.put(os, os, os.fill(), &t, pattern, pattern+2).failed();
        }
        catch (...)
        {
            failed = true;
        }
        if (failed)
            os.setstate(std::ios_base::failbit | std::ios_base::badbit);
    }
    return os;
}

}  // gregorian

#endif  // DATE_H
