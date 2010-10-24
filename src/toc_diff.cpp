#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <utility>
#include <iterator>
#include <stdexcept>
#include <map>
#include <algorithm>
#include <string>

std::string
remove_pending(std::string stat)
{
    typedef std::string::size_type size_type;
    if (stat.find("Pending") == 0)
        stat.erase(size_type(0), size_type(8));
    return stat;
}

std::string
remove_tentatively(std::string stat)
{
    typedef std::string::size_type size_type;
    if (stat.find("Tentatively") == 0)
        stat.erase(size_type(0), size_type(12));
    return stat;
}

std::string
remove_qualifier(std::string stat)
{
    return remove_tentatively(remove_pending(stat));
}

std::string
find_file(std::string stat)
{
    typedef std::string::size_type size_type;
    if (stat.find("Tentatively") == 0)
        return "lwg-active.html";
    stat = remove_qualifier(stat);
    if (stat == "TC1")
        return "lwg-defects.html";
    if (stat == "TC")
        return "lwg-defects.html";
    if (stat == "CD1")
        return "lwg-defects.html";
    if (stat == "WP")
        return "lwg-defects.html";
    if (stat == "DR")
        return "lwg-defects.html";
    if (stat == "TRDec")
        return "lwg-defects.html";
    if (stat == "Dup")
        return "lwg-closed.html";
    if (stat == "NAD")
        return "lwg-closed.html";
    if (stat == "NAD Future")
        return "lwg-closed.html";
    if (stat == "NAD_Future")
        return "lwg-closed.html";
    if (stat == "NAD Editorial")
        return "lwg-closed.html";
    if (stat == "NAD Concepts")
        return "lwg-closed.html";
    if (stat == "Ready")
        return "lwg-active.html";
    if (stat == "Tentatively Ready")
        return "lwg-active.html";
    if (stat == "Review")
        return "lwg-active.html";
    if (stat == "New")
        return "lwg-active.html";
    if (stat == "Open")
        return "lwg-active.html";
    throw std::runtime_error("unknown status " + stat);
}

bool
is_active(const std::string& stat)
{
    return find_file(stat) == "lwg-active.html";
}

std::vector<std::pair<int, std::string> >
read_issues(std::istream& is)
{
    std::vector<std::pair<int, std::string> > issues;
    std::istreambuf_iterator<char> first(is), last;
    std::string s(first, last);
    unsigned i = s.find("<tr>");
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find first row");
    while (true)
    {
        i = s.find("<tr>", i+4);
        if (i == std::string::npos)
            break;
        i = s.find("</a>", i);
        unsigned j = s.rfind('>', i);
        if (j == std::string::npos)
            throw std::runtime_error("unable to parse issue number: can't find beginning bracket");
        std::istringstream instr(s.substr(j+1, i-j-1));
        int num;
        instr >> num;
        if (instr.fail())
            throw std::runtime_error("unable to parse issue number");
        i = s.find("</a>", i+4);
        if (i == std::string::npos)
            throw std::runtime_error("partial issue found");
        j = s.rfind('>', i);
        if (j == std::string::npos)
            throw std::runtime_error("unable to parse issue status: can't find beginning bracket");
        std::string issue = s.substr(j+1, i-j-1);
        issues.push_back(std::make_pair(num, issue));
    }
    return issues;
}

void display_issues(const std::vector<std::pair<int, std::string> >& issues)
{
    for (std::vector<std::pair<int, std::string> >::const_iterator i = issues.begin(), e = issues.end(); i != e; ++i)
    {
        std::cout << i->first << " " << i->second << '\n';
    }
    std::cout << '\n';
}

struct find_num
{
    bool operator()(const std::pair<int, std::string>& x, int y)
    {
        return x.first < y;
    }
};

void discover_new_issues(const std::vector<std::pair<int, std::string> >& old_issues,
                         const std::vector<std::pair<int, std::string> >& new_issues)
{
    typedef std::vector<std::pair<int, std::string> >::const_iterator CI;
    std::map<std::string, std::vector<int> > added_issues;
    for (CI i = new_issues.begin(), e = new_issues.end(); i != e; ++i)
    {
        CI j = std::lower_bound(old_issues.begin(), old_issues.end(), i->first, find_num());
        if (j == old_issues.end())
            added_issues[i->second].push_back(i->first);
    }
    for (std::map<std::string, std::vector<int> >::const_iterator i = added_issues.begin(), e = added_issues.end(); i != e; ++i)
    {
        std::cout << "<li>Added the following " << i->first << " issues: ";
        std::cout << "<iref ref=\"" << i->second.front() << "\"/>";
        for (std::vector<int>::const_iterator j = i->second.begin()+1, e2 = i->second.end(); j != e2; ++j)
        {
            std::cout << ", <iref ref=\"" << *j << "\"/>";
        }
        std::cout << ".</li>\n";
    }
}

struct reverse_pair
{
    template <class T, class U>
    bool operator()(const T& x, const U& y)
    {
        return static_cast<bool>(x.second < y.second ||
	                      (!(y.second < x.second) && x.first < y.first));
    }
};

void discover_changed_issues(const std::vector<std::pair<int, std::string> >& old_issues,
                             const std::vector<std::pair<int, std::string> >& new_issues)
{
    typedef std::vector<std::pair<int, std::string> >::const_iterator CI;
    std::map<std::pair<std::string, std::string>, std::vector<int>, reverse_pair> changed_issues;
    for (CI i = new_issues.begin(), e = new_issues.end(); i != e; ++i)
    {
        CI j = std::lower_bound(old_issues.begin(), old_issues.end(), i->first, find_num());
        if (j != old_issues.end() && i->first == j->first && j->second != i->second)
            changed_issues[make_pair(j->second, i->second)].push_back(i->first);
    }
    for (std::map<std::pair<std::string, std::string>, std::vector<int>, reverse_pair>::const_iterator i = changed_issues.begin(), e = changed_issues.end(); i != e; ++i)
    {
        std::cout << "<li>Changed the following issues from " << i->first.first << " to " << i->first.second << ": ";
        std::cout << "<iref ref=\"" << i->second.front() << "\"/>";
        for (std::vector<int>::const_iterator j = i->second.begin()+1, e2 = i->second.end(); j != e2; ++j)
        {
            std::cout << ", <iref ref=\"" << *j << "\"/>";
        }
        std::cout << ".</li>\n";
    }
}

void count_issues(const std::vector<std::pair<int, std::string> >& issues, unsigned& n_open, unsigned& n_closed)
{
    typedef std::vector<std::pair<int, std::string> >::const_iterator CI;
    n_open = 0;
    n_closed = 0;
    for (CI i = issues.begin(), e = issues.end(); i != e; ++i)
    {
        if (is_active(i->second))
            ++n_open;
        else
            ++n_closed;
    }
}

void summary(const std::vector<std::pair<int, std::string> >& old_issues,
             const std::vector<std::pair<int, std::string> >& new_issues)
{
    unsigned n_open_new = 0;
    unsigned n_open_old = 0;
    unsigned n_closed_new = 0;
    unsigned n_closed_old = 0;
    count_issues(old_issues, n_open_old, n_closed_old);
    count_issues(new_issues, n_open_new, n_closed_new);

    std::cout << "<li>" << n_open_new << " open issues, ";
    if (n_open_new >= n_open_old)
        std::cout << "up by " << n_open_new - n_open_old << ".</li>\n";
    else
        std::cout << "down by " << n_open_old - n_open_new << ".</li>\n";

    std::cout << "<li>" << n_closed_new << " closed issues, ";
    if (n_closed_new >= n_closed_old)
        std::cout << "up by " << n_closed_new - n_closed_old << ".</li>\n";
    else
        std::cout << "down by " << n_closed_old - n_closed_new << ".</li>\n";

    unsigned n_total_new = n_open_new + n_closed_new;
    unsigned n_total_old = n_open_old + n_closed_old;
    std::cout << "<li>" << n_total_new << " issues total, ";
    if (n_total_new >= n_total_old)
        std::cout << "up by " << n_total_new - n_total_old << ".</li>\n";
    else
        std::cout << "down by " << n_total_old - n_total_new << ".</li>\n";
}

int main (int argc, char* const argv[])
{
    std::vector<std::pair<int, std::string> > old_issues;
    std::vector<std::pair<int, std::string> > new_issues;
    {
        std::ifstream old_html("lwg-toc.old.html");
        if (!old_html.is_open())
        {
            std::cout << "Unable to open lwg-toc.old.html\n";
            return 1;
        }
        old_issues = read_issues(old_html);
    }
//    display_issues(old_issues);
    {
        std::ifstream new_html("lwg-toc.html");
        if (!new_html.is_open())
        {
            std::cout << "Unable to open lwg-toc.html\n";
            return 1;
        }
        new_issues = read_issues(new_html);
    }
//    display_issues(new_issues);
    std::cout << "<ul>\n";
    std::cout << "<li><b>Summary:</b><ul>\n";
    summary(old_issues, new_issues);
    std::cout << "</ul></li>\n";
    std::cout << "<li><b>Details:</b><ul>\n";
    discover_new_issues(old_issues, new_issues);
    discover_changed_issues(old_issues, new_issues);
    std::cout << "</ul></li>\n";
    std::cout << "</ul>\n";

}
