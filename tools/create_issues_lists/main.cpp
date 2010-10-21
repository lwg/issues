#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <iterator>
#include <stdexcept>
#include <cctype>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "date.h"

namespace greg = gregorian;

std::string insert_color("ins {background-color:#A0FFA0}\n");
std::string delete_color("del {background-color:#FFA0A0}\n");

std::string path;
std::string issues_path;

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
    if (stat == "NAD Editorial")
        return "lwg-closed.html";
    if (stat == "NAD Concepts")
        return "lwg-closed.html";
    if (stat == "Ready")
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
is_active_not_ready(const std::string& stat)
{
    return find_file(stat) == "lwg-active.html" && stat != "Ready";
}

bool
is_active(const std::string& stat)
{
    return find_file(stat) == "lwg-active.html";
}

bool
is_defect(const std::string& stat)
{
    return find_file(stat) == "lwg-defects.html";
}

bool
is_closed(const std::string& stat)
{
    return find_file(stat) == "lwg-closed.html";
}

struct section_num
{
    std::string prefix;
    std::vector<int> num;

};

std::istream&
operator >> (std::istream& is, section_num& sn)
{
    sn.prefix.clear();
    sn.num.clear();
    ws(is);
    if (is.peek() == 'T')
    {
        is.get();
        if (is.peek() == 'R')
        {
            std::string w;
            is >> w;
            if (w == "R1")
                sn.prefix = "TR1";
            else if (w == "RDecimal")
                sn.prefix = "TRDecimal";
            else
                throw std::runtime_error("section_num format error");
            ws(is);
        }
        else
        {
            sn.num.push_back(100 + 'T' - 'A');
            if (is.peek() != '.')
                return is;
             is.get();
        }
    }
    while (true)
    {
        if (std::isdigit(is.peek()))
        {
            int n;
            is >> n;
            sn.num.push_back(n);
        }
        else
        {
            char c;
            is >> c;
            sn.num.push_back(100 + c - 'A');
        }
        if (is.peek() != '.')
            break;
        char dot;
        is >> dot;
    }
    return is;
}

std::ostream&
operator << (std::ostream& os, const section_num& sn)
{
    if (!sn.prefix.empty())
        os << sn.prefix << " ";
    if (sn.num.size() > 0)
    {
        if (sn.num.front() >= 100)
            os << char(sn.num.front() - 100 + 'A');
        else
            os << sn.num.front();
        for (unsigned i = 1; i < sn.num.size(); ++i)
        {
            os << '.';
            if (sn.num[i] >= 100)
                os << char(sn.num[i] - 100 + 'A');
            else
                os << sn.num[i];
        }
    }
    return os;
}

bool
operator<(const section_num& x, const section_num& y)
{
    if (x.prefix < y.prefix)
        return true;
    else if (y.prefix < x.prefix)
        return false;
    return x.num < y.num;
}

bool
operator==(const section_num& x, const section_num& y)
{
    if (x.prefix != y.prefix)
        return false;
    return x.num == y.num;
}

bool
operator!=(const section_num& x, const section_num& y)
{
    return !(x == y);
}

typedef std::string section_tag;

struct issue
{
    int num;
    std::string stat;
    std::string title;
    std::vector<section_tag> tags;
    std::string submitter;
    greg::date date;
    greg::date mod_date;
    std::vector<std::string> duplicates;
    std::string text;
    bool has_resolution;
};

typedef std::map<section_tag, section_num> SectionMap;
SectionMap section_db;

section_tag
remove_square_brackets(const section_tag& tag)
{
    assert(tag.size() > 2);
    assert(tag[0] == '[');
    assert(tag[tag.size()-1] == ']');
    return tag.substr(1, tag.size()-2);
}

struct sort_by_section
{
    bool operator()(const issue& x, const issue& y)
    {
        return section_db[x.tags[0]] < section_db[y.tags[0]];
    }
};

struct sort_by_num
{
    bool operator()(const issue& x, const issue& y) const
    {
        return x.num < y.num;
    }
    bool operator()(const issue& x, int y) const
    {
        return x.num < y;
    }
    bool operator()(int x, const issue& y) const
    {
        return x < y.num;
    }
};

struct sort_by_status
{
    bool operator()(const issue& x, const issue& y) const
    {
        if (is_active(x.stat) && !is_active(y.stat))
            return true;
        if (!is_active(x.stat) && is_active(y.stat))
            return false;
        if (is_active(x.stat) && is_active(y.stat))
        {
            if (x.stat == "Ready" && y.stat == "Ready")
                return false;
            if (x.stat == "Ready" && y.stat != "Ready")
                return false;
            if (x.stat != "Ready" && y.stat == "Ready")
                return true;
        }
        return x.stat < y.stat;
    }
};

struct sort_by_first_tag
{
    bool operator()(const issue& x, const issue& y) const
    {
        return x.tags[0] < y.tags[0];
    }
};

struct sort_by_mod_date
{
    bool operator()(const issue& x, const issue& y) const
    {
        return x.mod_date > y.mod_date;
    }
};

struct equal_issue_num
{
    bool operator()(const issue& x, int y) {return x.num == y;}
    bool operator()(int x, const issue& y) {return x == y.num;}
};

/*
void display(const std::vector<issue>& issues)
{
    for (unsigned i = 0; i < issues.size(); ++i)
        std::cout << issues[i];
}
*/
SectionMap
read_section_db()
{
    SectionMap section_db;
    std::ifstream infile((path + "section.data").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Can't open section.data\n");
    while (infile)
    {
        ws(infile);
        std::string line;
        getline(infile, line);
        if (!line.empty())
        {
            assert(line[line.size()-1] == ']');
            unsigned p = line.rfind('[');
            assert(p != std::string::npos);
            section_tag tag = line.substr(p);
            assert(tag.size() > 2);
            assert(tag[0] == '[');
            assert(tag[tag.size()-1] == ']');
            line.erase(p-1);
            section_num num;
            if (tag.find("[trdec.") != std::string::npos)
            {
                num.prefix = "TRDecimal";
                line.erase(0, 10);
            }
            else if (tag.find("[tr.") != std::string::npos)
            {
                num.prefix = "TR1";
                line.erase(0, 4);
            }
            std::istringstream temp(line);
            if (!std::isdigit(line[0]))
            {
                char c;
                temp >> c;
                num.num.push_back(100 + c - 'A');
                temp >> c;
            }
            while (temp)
            {
                int n;
                temp >> n;
                if (!temp.fail())
                {
                    num.num.push_back(n);
                    char dot;
                    temp >> dot;
                }
            }
            section_db[tag] = num;
        }
    }
    return section_db;
}

void
check_against_index(const SectionMap& section_db)
{
    SectionMap::const_iterator s_db = section_db.begin();
    SectionMap::const_iterator e_db = section_db.end();
    for (SectionMap::const_iterator s_db = section_db.begin(), e_db = section_db.end(); s_db != e_db; ++s_db)
    {
        std::string temp = s_db->first;
        temp.erase(temp.end()-1);
        temp.erase(temp.begin());
        std::cout << temp << ' ' << s_db->second << '\n';
    }
}

std::string
get_revision()
{
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find("revision=\"");
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find revision in lwg-issues.xml");
    i += sizeof("revision=\"") - 1;
    unsigned j = s.find('\"', i);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to parse revision in lwg-issues.xml");
    return s.substr(i, j-i);
}

std::string
get_maintainer()
{
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find("maintainer=\"");
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find maintainer in lwg-issues.xml");
    i += sizeof("maintainer=\"") - 1;
    unsigned j = s.find('\"', i);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to parse maintainer in lwg-issues.xml");
    std::string r = s.substr(i, j-i);
    std::size_t m = r.find("&lt;");
    if (m == std::string::npos)
        throw std::runtime_error("Unable to parse maintainer email address in lwg-issues.xml");
    m += sizeof("&lt;") - 1;
    std::size_t me = r.find("&gt;", m);
    if (me == std::string::npos)
        throw std::runtime_error("Unable to parse maintainer email address in lwg-issues.xml");
    std::string email = r.substr(m, me-m);
    // &lt;                                          howard.hinnant@gmail.com    &gt;
    // &lt;<a href="mailto:howard.hinnant@gmail.com">howard.hinnant@gmail.com</a>&gt;
    r.replace(m, me-m, "<a href=\"mailto:" + email + "\">" + email + "</a>");
    return r;
}

std::string
get_date()
{
#if 1
    greg::date today;
    std::ostringstream date;
    date << today.year() << '-';
    if (today.month() < 10)
        date << '0';
    date << today.month() << '-';
    if (today.day() < 10)
        date << '0';
    date << today.day();
    return date.str();
#else
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find("date=\"");
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find date in lwg-issues.xml");
    i += sizeof("date=\"") - 1;
    unsigned j = s.find('\"', i);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to parse date in lwg-issues.xml");
    return s.substr(i, j-i);
#endif
}

std::string
get_doc_number(std::string doc)
{
    if (doc == "active")
        doc = "active_docno=\"";
    else if (doc == "defect")
        doc = "defect_docno=\"";
    else if (doc == "closed")
        doc = "closed_docno=\"";
    else
        throw std::runtime_error("unknown argument to get_doc_number: " + doc);
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find(doc);
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find docno in lwg-issues.xml");
    i += doc.size();
    unsigned j = s.find('\"', i+1);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to parse docno in lwg-issues.xml");
    return s.substr(i, j-i);
}

std::string
get_intro(std::string doc)
{
    if (doc == "active")
        doc = "<intro list=\"Active\">";
    else if (doc == "defect")
        doc = "<intro list=\"Defects\">";
    else if (doc == "closed")
        doc = "<intro list=\"Closed\">";
    else
        throw std::runtime_error("unknown argument to intro: " + doc);
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find(doc);
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find intro in lwg-issues.xml");
    i += doc.size();
    unsigned j = s.find("</intro>", i);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to parse intro in lwg-issues.xml");
    return s.substr(i, j-i);
}

void format(std::vector<issue>& issues, issue& is)
{
    std::string& s = is.text;
    int issue_num = is.num;
    std::vector<std::string> tag_stack;
    std::ostringstream er;
    for (unsigned i = 0; i < s.size(); ++i)
    {
        if (s[i] == '<')
        {
            unsigned j = s.find('>', i);
            if (j == std::string::npos)
            {
                er.clear();
                er.str("");
                er << "missing '>' in issue " << issue_num;
                throw std::runtime_error(er.str());
            }
            std::string tag;
            {
            std::istringstream iword(s.substr(i+1, j-i-1));
            iword >> tag;
            }
            if (tag.empty())
            {
                er.clear();
                er.str("");
                er << "unexpected <> in issue " << issue_num;
                throw std::runtime_error(er.str());
            }
            if (tag[0] == '/')  // closing tag
            {
                tag.erase(tag.begin());
                if (tag == "issue" || tag == "revision")
                {
                    s.erase(i, j-i + 1);
                    --i;
                    return;
                }
                if (tag_stack.empty() || tag != tag_stack.back())
                {
                    er.clear();
                    er.str("");
                    er << "mismatched tags in issue " << issue_num;
                    if (tag_stack.empty())
                        er << ".  Had no open tag.";
                    else
                        er << ".  Open tag was " << tag_stack.back() << ".";
                    er << "  Closing tag was " << tag;
                    throw std::runtime_error(er.str());
                }
                tag_stack.pop_back();
                if (tag == "discussion")
                {
                    s.erase(i, j-i + 1);
                    --i;
                }
                else if (tag == "resolution")
                {
                    s.erase(i, j-i + 1);
                    --i;
                }
                else if (tag == "rationale")
                {
                    s.erase(i, j-i + 1);
                    --i;
                }
                else if (tag == "duplicate")
                {
                    s.erase(i, j-i + 1);
                    --i;
                }
                else if (tag == "note")
                {
                    s.replace(i, j-i + 1, "]</i></p>\n");
                    i += 9;
                }
                else
                    i = j;
                continue;
            }
            if (s[j-1] == '/')  // self-contained tag: sref, iref
            {
                if (tag == "sref")
                {
                    std::string r;
                    unsigned k = s.find('\"', i+5);
                    if (k >= j)
                    {
                        er.clear();
                        er.str("");
                        er << "missing '\"' in sref in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    unsigned l = s.find('\"', k+1);
                    if (l >= j)
                    {
                        er.clear();
                        er.str("");
                        er << "missing '\"' in sref in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    ++k;
                    r = s.substr(k, l-k);
//                     if (section_db.count(r) == 0)
//                     {
//                         er.clear();
//                         er.str("");
//                         er << "unknown section reference: " << r << " in issue " << issue_num;
//                         std::cout << er.str() << '\n';
//                     }
                    {
                    std::ostringstream t;
                    t << section_db[r] << ' ';
                    r.insert(0, t.str());
                    }
                    j -= i - 1;
                    s.replace(i, j, r);
                    i += r.size() - 1;
                    continue;
                }
                else if (tag == "iref")
                {
                    std::string r;
                    unsigned k = s.find('\"', i+5);
                    if (k >= j)
                    {
                        er.clear();
                        er.str("");
                        er << "missing '\"' in iref in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    unsigned l = s.find('\"', k+1);
                    if (l >= j)
                    {
                        er.clear();
                        er.str("");
                        er << "missing '\"' in iref in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    ++k;
                    r = s.substr(k, l-k);
                    std::istringstream temp(r);
                    int num;
                    temp >> num;
                    if (temp.fail())
                    {
                        er.clear();
                        er.str("");
                        er << "bad number in iref in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    std::vector<issue>::iterator n = std::lower_bound(issues.begin(), issues.end(), num, sort_by_num());
                    if (n->num != num)
                    {
                        er.clear();
                        er.str("");
                        er << "couldn't find number in iref in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    if (!tag_stack.empty() && tag_stack.back() == "duplicate")
                    {
                        std::ostringstream temp;
                        temp << is.num;
                        std::string d = "<a href=\"";
                        d += find_file(is.stat);
                        d += '#';
                        d += temp.str();
                        d += "\">";
                        d += temp.str();
                        d += "</a>";
                        n->duplicates.push_back(d);
                        temp.str("");
                        temp << n->num;
                        d = "<a href=\"";
                        d += find_file(n->stat);
                        d += '#';
                        d += temp.str();
                        d += "\">";
                        d += temp.str();
                        d += "</a>";
                        is.duplicates.push_back(d);
                        r.clear();
                    }
                    else
                    {
                        r = "<a href=\"";
                        r += find_file(n->stat);
                        r += '#';
                        std::string ns;
                        {
                        std::ostringstream t;
                        t << num;
                        ns = t.str();
                        }
                        r += ns;
                        r += "\">";
                        r += ns;
                        r += "</a>";
                    }
                    j -= i - 1;
                    s.replace(i, j, r);
                    i += r.size() - 1;
                    continue;
                }
                else if (tag == "cd_N2800")
                {
                    assert(!"not implemented yet");
                    std::string r;
                    unsigned k = s.find('\"', i+8);
                    if (k >= j)
                    {
                        er.clear();
                        er.str("");
                        er << "missing '\"' in cd_N2800 in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    unsigned l = s.find('\"', k+1);
                    if (l >= j)
                    {
                        er.clear();
                        er.str("");
                        er << "missing '\"' in cd_N2800 in issue " << issue_num;
                        throw std::runtime_error(er.str());
                    }
                    ++k;
                    r = s.substr(k, l-k);
                    std::istringstream temp(r);
                    std::string country;
                    int cd_issue_num;
                    temp >> country >> cd_issue_num;
                    
                    j -= i - 1;
                    s.replace(i, j, r);
                    i += r.size() - 1;
                    continue;
                }
                i = j;
                continue;  // don't worry about this <tag/>
            }
            tag_stack.push_back(tag);
            if (tag == "discussion")
            {
                s.replace(i, j-i + 1, "<p><b>Discussion:</b></p>");
                i += 24;
            }
            else if (tag == "resolution")
            {
                s.replace(i, j-i + 1, "<p><b>Proposed resolution:</b></p>");
                i += 33;
            }
            else if (tag == "rationale")
            {
                s.replace(i, j-i + 1, "<p><b>Rationale:</b></p>");
                i += 23;
            }
            else if (tag == "duplicate")
            {
                s.erase(i, j-i + 1);
                --i;
            }
            else if (tag == "note")
            {
                s.replace(i, j-i + 1, "<p><i>[");
                i += 6;
            }
            else if (tag == "!--")
            {
                tag_stack.pop_back();
                j = s.find("-->", i);
                j += 3;
                s.erase(i, j-i);
                --i;
            }
            else
                i = j;
        }
    }
}

std::string
get_revisions(std::vector<issue>& issues)
{
    std::string r = "<ul>\n";
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find("<revision_history>");
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find <revision_history> in lwg-issues.xml");
    i += sizeof("<revision_history>") - 1;
    unsigned j = s.find("</revision_history>", i);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to find </revision_history> in lwg-issues.xml");
    s = s.substr(i, j-i);
    j = 0;
    while (true)
    {
        i = s.find("<revision tag=\"", j);
        if (i == std::string::npos)
            break;
        i += sizeof("<revision tag=\"") - 1;
        j = s.find('\"', i);
        std::string rv = s.substr(i, j-i);
        i = j+2;
        j = s.find("</revision>", i);
        issue is;
        is.text = s.substr(i, j-i);
        format(issues, is);
        r += "<li>";
        r += rv + ": ";
        r += is.text;
        r += "</li>\n";
    }
    r += "</ul>\n";
    return r;
}

std::string
get_statuses()
{
    std::ifstream infile((issues_path + "lwg-issues.xml").c_str());
    if (!infile.is_open())
        throw std::runtime_error("Unable to open lwg-issues.xml");
    std::istreambuf_iterator<char> first(infile), last;
    std::string s(first, last);
    unsigned i = s.find("<statuses>");
    if (i == std::string::npos)
        throw std::runtime_error("Unable to find statuses in lwg-issues.xml");
    i += sizeof("<statuses>") - 1;
    unsigned j = s.find("</statuses>", i);
    if (j == std::string::npos)
        throw std::runtime_error("Unable to parse statuses in lwg-issues.xml");
    return s.substr(i, j-i);
}

void
print_table(std::ostream& out, std::vector<issue>::const_iterator i, std::vector<issue>::const_iterator e)
{
    std::string prev_tag;
    for (; i != e; ++i)
    {
        out << "<tr>\n";

        // Number
        out << "<td align=\"right\">";
        out << "<a href=\"";
        out << find_file(i->stat);
        out << '#';
        out << i->num;
        out << "\">";
        out << i->num;
        out << "</a>";
        out << "</td>\n";

        // Status
        out << "<td align=\"left\"><a href=\"lwg-active.html#";
        out << remove_qualifier(i->stat);
        out << "\">";
        out << i->stat;
        out << "</a>";
        out << "<a name=\"" << i->num << "\"></a>";
        out << "</td>\n";

        // Section
        out << "<td align=\"left\">";
        out << section_db[i->tags[0]] << " " << i->tags[0];
        if (i->tags[0] != prev_tag)
        {
            prev_tag = i->tags[0];
            out << "<a name=\"" << remove_square_brackets(prev_tag) << "\"</a>";
        }
        out << "</td>\n";

        // Title
        out << "<td align=\"left\">";
        out << i->title;
        out << "</td>\n";

        // Has Proposed Resolution
        out << "<td align=\"center\">";
        if (i->has_resolution)
            out << "Yes";
        else
            out << "<font color=\"red\">No</font>";
        out << "</td>\n";

        // Duplicates
        out << "<td align=\"left\">";
        if (!i->duplicates.empty())
        {
            out << i->duplicates[0];
            for (unsigned j = 1; j < i->duplicates.size(); ++j)
                out << ", " << i->duplicates[j];
        }
        out << "</td>\n";

        // Modification date
        out << "<td align=\"center\">";
        out << i->mod_date.year() << '-';
        if (i->mod_date.month() < 10)
            out << '0';
        out << i->mod_date.month() << '-';
        if (i->mod_date.day() < 10)
            out << '0';
        out << i->mod_date.day() << "</p>\n";
        out << "</td>\n";

        out << "</tr>\n";
    }
    out << "</table>\n";
}

void print_file_header(std::ostream& out, const std::string& title)
{
    out << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n";
    out << "    \"http://www.w3.org/TR/html4/strict.dtd\">\n";
    out << "<html>\n";
    out << "<head>\n";
    out << "<title>" << title << "</title>\n";
    out << "<style type=\"text/css\">\n";
    out << "p {text-align:justify}\n";
    out << "li {text-align:justify}\n";
    out << "blockquote.note\n";
    out << "{\n";
    out << "    background-color:#E0E0E0;\n";
    out << "    padding-left: 15px;\n";
    out << "    padding-right: 15px;\n";
    out << "    padding-top: 1px;\n";
    out << "    padding-bottom: 1px;\n";
    out << "}\n";
    out << insert_color;
    out << delete_color;
    out << "</style>\n";
    out << "</head>\n";
    out << "<body>\n";
}

void print_file_trailer(std::ostream& out)
{
    out << "</body>\n";
    out << "</html>\n";
}

void make_sort_by_num(std::vector<issue>& issues)
{
    std::ofstream out((path + "lwg-toc.html").c_str());
    out << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n";
    out << "    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
    out << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
    out << "<head>\n";
    out << "<title>Library Issues List Table of Contents</title>\n";
    out << "<style type=\"text/css\">\n";
    out << "p {text-align:justify}\n";
    out << "li {text-align:justify}\n";
    out << "blockquote.note\n";
    out << "{\n";
    out << "    background-color:#E0E0E0;\n";
    out << "    padding-left: 15px;\n";
    out << "    padding-right: 15px;\n";
    out << "    padding-top: 1px;\n";
    out << "    padding-bottom: 1px;\n";
    out << "}\n";
    out << insert_color;
    out << delete_color;
    out << "</style>\n";
    out << "</head>\n";
    out << "<body>\n";

    out << "<h1>C++ Standard Library Issues List (Revision " << get_revision() << ")</h1>\n";
    out << "<h1>Table of Contents</h1>\n";
    out << "<p>Reference ISO/IEC IS 14882:2003(E)</p>\n";
    out << "<p>This document is the Table of Contents for the <a href=\"lwg-active.html\">Library Active Issues List</a>,"
           " <a href=\"lwg-defects.html\">Library Defect Reports List</a>, and <a href=\"lwg-closed.html\">Library Closed Issues List</a>.</p>\n";
    out << "<h2>Table of Contents</h2>\n";

    out << "<table border=\"1\" cellpadding=\"4\">\n";
    out << "<tr>\n";
    out << "<td align=\"center\"><b>Issue</b></td>\n";
    out << "<td align=\"center\"><a href=\"lwg-status.html\"><b>Status</b></a></td>\n";
    out << "<td align=\"center\"><a href=\"lwg-index.html\"><b>Section</b></a></td>\n";
    out << "<td align=\"center\"><b>Title</b></td>\n";
    out << "<td align=\"center\"><b>Proposed Resolution</b></td>\n";
    out << "<td align=\"center\"><b>Duplicates</b></td>\n";
    out << "<td align=\"center\"><a href=\"lwg-status-date.html\"><b>Last modified</b></a></td>\n";

    sort(issues.begin(), issues.end(), sort_by_num());
    print_table(out, issues.begin(), issues.end());

    out << "</body>\n";
    out << "</html>\n";

}

void make_sort_by_status(std::vector<issue>& issues)
{
    std::ofstream out((path + "lwg-status.html").c_str());
    out << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n";
    out << "    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
    out << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
    out << "<head>\n";
    out << "<title>Library Issues List Table of Contents</title>\n";
    out << "<style type=\"text/css\">\n";
    out << "p {text-align:justify}\n";
    out << "li {text-align:justify}\n";
    out << "blockquote.note\n";
    out << "{\n";
    out << "    background-color:#E0E0E0;\n";
    out << "    padding-left: 15px;\n";
    out << "    padding-right: 15px;\n";
    out << "    padding-top: 1px;\n";
    out << "    padding-bottom: 1px;\n";
    out << "}\n";
    out << insert_color;
    out << delete_color;
    out << "</style>\n";
    out << "</head>\n";
    out << "<body>\n";

    out << "<h1>C++ Standard Library Issues List (Revision " << get_revision() << ")</h1>\n";
    out << "<h1>Table of Contents</h1>\n";
    out << "<p>Reference ISO/IEC IS 14882:2003(E)</p>\n";
    out << "<p>This document is the Table of Contents for the <a href=\"lwg-active.html\">Library Active Issues List</a>,"
           " <a href=\"lwg-defects.html\">Library Defect Reports List</a>, and <a href=\"lwg-closed.html\">Library Closed Issues List</a>.</p>\n";

    out << "<h2>Index by Status</h2>\n";

    sort(issues.begin(), issues.end(), sort_by_num());
    stable_sort(issues.begin(), issues.end(), sort_by_mod_date());
    stable_sort(issues.begin(), issues.end(), sort_by_section());
    stable_sort(issues.begin(), issues.end(), sort_by_status());

    for (std::vector<issue>::const_iterator i = issues.begin(), e = issues.end(); i != e;)
    {

        std::string current_status = i->stat;
        std::vector<issue>::const_iterator j = i;
        for (; j != e; ++j)
        {
            if (j->stat != current_status)
                break;
        }
        out << "<h2><a name=\"" << current_status << "\"</a>" << current_status << " (" << (j-i) << " issues)</h2>\n";

        out << "<table border=\"1\" cellpadding=\"4\">\n";
        out << "<tr>\n";
        out << "<td align=\"center\"><a href=\"lwg-toc.html\"><b>Issue</b></a></td>\n";
        out << "<td align=\"center\"><b>Status</b></td>\n";
        out << "<td align=\"center\"><a href=\"lwg-index.html\"><b>Section</b></a></td>\n";
        out << "<td align=\"center\"><b>Title</b></td>\n";
        out << "<td align=\"center\"><b>Proposed Resolution</b></td>\n";
        out << "<td align=\"center\"><b>Duplicates</b></td>\n";
        out << "<td align=\"center\"><a href=\"lwg-status-date.html\"><b>Last modified</b></a></td>\n";

        print_table(out, i, j);
        i = j;
    }

    out << "</body>\n";
    out << "</html>\n";

}

void make_sort_by_status_mod_date(std::vector<issue>& issues)
{
    std::ofstream out((path + "lwg-status-date.html").c_str());
    out << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n";
    out << "    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
    out << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
    out << "<head>\n";
    out << "<title>Library Issues List Table of Contents</title>\n";
    out << "<style type=\"text/css\">\n";
    out << "p {text-align:justify}\n";
    out << "li {text-align:justify}\n";
    out << "blockquote.note\n";
    out << "{\n";
    out << "    background-color:#E0E0E0;\n";
    out << "    padding-left: 15px;\n";
    out << "    padding-right: 15px;\n";
    out << "    padding-top: 1px;\n";
    out << "    padding-bottom: 1px;\n";
    out << "}\n";
    out << insert_color;
    out << delete_color;
    out << "</style>\n";
    out << "</head>\n";
    out << "<body>\n";

    out << "<h1>C++ Standard Library Issues List (Revision " << get_revision() << ")</h1>\n";
    out << "<h1>Table of Contents</h1>\n";
    out << "<p>Reference ISO/IEC IS 14882:2003(E)</p>\n";
    out << "<p>This document is the Table of Contents for the <a href=\"lwg-active.html\">Library Active Issues List</a>,"
           " <a href=\"lwg-defects.html\">Library Defect Reports List</a>, and <a href=\"lwg-closed.html\">Library Closed Issues List</a>.</p>\n";

    out << "<h2>Index by Status</h2>\n";

    sort(issues.begin(), issues.end(), sort_by_num());
    stable_sort(issues.begin(), issues.end(), sort_by_section());
    stable_sort(issues.begin(), issues.end(), sort_by_mod_date());
    stable_sort(issues.begin(), issues.end(), sort_by_status());

    for (std::vector<issue>::const_iterator i = issues.begin(), e = issues.end(); i != e;)
    {

        std::string current_status = i->stat;
        std::vector<issue>::const_iterator j = i;
        for (; j != e; ++j)
        {
            if (j->stat != current_status)
                break;
        }
        out << "<h2><a name=\"" << current_status << "\"</a>" << current_status << " (" << (j-i) << " issues)</h2>\n";

        out << "<table border=\"1\" cellpadding=\"4\">\n";
        out << "<tr>\n";
        out << "<td align=\"center\"><a href=\"lwg-toc.html\"><b>Issue</b></a></td>\n";
        out << "<td align=\"center\"><a href=\"lwg-status.html\"><b>Status</b></a></td>\n";
        out << "<td align=\"center\"><a href=\"lwg-index.html\"><b>Section</b></a></td>\n";
        out << "<td align=\"center\"><b>Title</b></td>\n";
        out << "<td align=\"center\"><b>Proposed Resolution</b></td>\n";
        out << "<td align=\"center\"><b>Duplicates</b></td>\n";
        out << "<td align=\"center\"><b>Last modified</b></td></tr>\n";

        print_table(out, i, j);
        i = j;
    }

    out << "</body>\n";
    out << "</html>\n";

}

std::string
major_section(const section_num& sn)
{
    std::ostringstream out;
    std::string prefix = sn.prefix;
    if (!prefix.empty())
        out << prefix << " ";
    if (sn.num[0] < 100)
        out << sn.num[0];
    else
        out << char(sn.num[0] - 100 + 'A');
    return out.str();
}

struct sort_by_mjr_section
{
    bool operator()(const issue& x, const issue& y) const
    {
        const section_num& xn = section_db[x.tags[0]];
        const section_num& yn = section_db[y.tags[0]];
        if (xn.prefix < yn.prefix)
            return true;
        if (xn.prefix > yn.prefix)
            return false;
        return xn.num[0] < yn.num[0];
    }
};

void make_sort_by_section(std::vector<issue>& issues, bool active_only = false)
{
    std::string filename;
    if (active_only)
        filename = path + "lwg-index-open.html";
    else
        filename = path + "lwg-index.html";
    std::ofstream out(filename.c_str());
    out << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n";
    out << "    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n";
    out << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
    out << "<head>\n";
    out << "<title>Library Issues List Table of Contents</title>\n";
    out << "<style type=\"text/css\">\n";
    out << "p {text-align:justify}\n";
    out << "li {text-align:justify}\n";
    out << "blockquote.note\n";
    out << "{\n";
    out << "    background-color:#E0E0E0;\n";
    out << "    padding-left: 15px;\n";
    out << "    padding-right: 15px;\n";
    out << "    padding-top: 1px;\n";
    out << "    padding-bottom: 1px;\n";
    out << "}\n";
    out << insert_color;
    out << delete_color;
    out << "</style>\n";
    out << "</head>\n";
    out << "<body>\n";

    out << "<h1>C++ Standard Library Issues List (Revision " << get_revision() << ")</h1>\n";
    out << "<h1>Table of Contents</h1>\n";
    out << "<p>Reference ISO/IEC IS 14882:2003(E)</p>\n";
    out << "<p>This document is the Table of Contents for the <a href=\"lwg-active.html\">Library Active Issues List</a>,"
           " <a href=\"lwg-defects.html\">Library Defect Reports List</a>, and <a href=\"lwg-closed.html\">Library Closed Issues List</a>.</p>\n";
    out << "<h2>Index by Section";
    if (active_only)
        out << " (non-Ready active issues only)";
    out << "</h2>\n";
    if (active_only)
        out << "<p><a href=\"lwg-index.html\">(view all issues)</a></p>\n";
    else
        out << "<p><a href=\"lwg-index-open.html\">(view only non-Ready open issues)</a></p>\n";
    sort(issues.begin(), issues.end(), sort_by_num());
    stable_sort(issues.begin(), issues.end(), sort_by_mod_date());
    stable_sort(issues.begin(), issues.end(), sort_by_status());
    std::vector<issue>::iterator e = issues.end();
    if (active_only)
    {
        std::vector<issue>::iterator j;
        for (j = issues.begin(); j != e; ++j)
        {
            if (!is_active_not_ready(j->stat))
                break;
        }
        e = j;
    }
    stable_sort(issues.begin(), e, sort_by_section());
    std::set<issue, sort_by_mjr_section> mjr_section_open;
    for (std::vector<issue>::const_iterator i = issues.begin(); i != e; ++i)
        if (is_active_not_ready(i->stat))
            mjr_section_open.insert(*i);

    for (std::vector<issue>::const_iterator i = issues.begin(); i != e;)
    {
        int current_num = section_db[i->tags[0]].num[0];
        std::vector<issue>::const_iterator j = i;
        for (; j != e; ++j)
        {
            if (section_db[j->tags[0]].num[0] != current_num)
                break;
        }
        std::string msn = major_section(section_db[i->tags[0]]);
        out << "<h2><a name=\"Section " << msn << "\"></a>" << "Section " << msn;
        out << " (" << (j-i) << " issues)</h2>\n";
        out << "<p>";
        if (active_only)
            out << "<a href=\"lwg-index.html#Section " << msn << "\">(view all issues)</a>";
        else if (mjr_section_open.count(*i) > 0)
            out << "<a href=\"lwg-index-open.html#Section " << msn << "\">(view only non-Ready open issues)</a>";
        out << "</p>\n";

        out << "<table border=\"1\" cellpadding=\"4\">\n";
        out << "<tr>\n";
        out << "<td align=\"center\"><a href=\"lwg-toc.html\"><b>Issue</b></a></td>\n";
        out << "<td align=\"center\"><a href=\"lwg-status.html\"><b>Status</b></a></td>\n";
        out << "<td align=\"center\"><b>Section</b></td>\n";
        out << "<td align=\"center\"><b>Title</b></td>\n";
        out << "<td align=\"center\"><b>Proposed Resolution</b></td>\n";
        out << "<td align=\"center\"><b>Duplicates</b></td>\n";
        out << "<td align=\"center\"><a href=\"lwg-status-date.html\"><b>Last modified</b></a></td>\n";

        print_table(out, i, j);
        i = j;
    }

    out << "</body>\n";
    out << "</html>\n";

}

struct if_active
{
    bool operator()(const issue& i) {return is_active(i.stat);}
};

template <class Pred>
void
print_issues(std::ostream& out, const std::vector<issue>& issues, Pred pred)
{
    std::multiset<issue, sort_by_first_tag> all_issues(issues.begin(), issues.end());
    std::multiset<issue, sort_by_first_tag> active_issues;
    for (std::vector<issue>::const_iterator i = issues.begin(), e = issues.end(); i != e; ++i)
    {
        if (if_active()(*i))
            active_issues.insert(*i);
    }
    std::multiset<issue, sort_by_status> issues_by_status(issues.begin(), issues.end());
    for (std::vector<issue>::const_iterator i = issues.begin(), e = issues.end(); i != e; ++i)
    {
        if (pred(*i))
        {
            out << "<hr>\n";
    
            // Number and title
            out << "<h3>";
            out << "<a name=\"";
            out << i->num;
            out << "\"></a>";
            out << i->num << ". " << i->title << "</h3>\n";
    
            // Section, Status, Submitter, Date
            out << "<p><b>Section:</b> ";
            out << section_db[i->tags[0]] << " " << i->tags[0];
            for (unsigned k = 1; k < i->tags.size(); ++k)
                out << ", " << section_db[i->tags[k]] << " " << i->tags[k];
            out << " <b>Status:</b> <a href=\"lwg-active.html#" << remove_qualifier(i->stat) << "\">" << i->stat << "</a>\n";
            out << " <b>Submitter:</b> " << i->submitter;
            out << " <b>Opened:</b> " << i->date.year() << '-';
            if (i->date.month() < 10)
                out << '0';
            out << i->date.month() << '-';
            if (i->date.day() < 10)
                out << '0';
            out << i->date.day() << " ";
            out << " <b>Last modified:</b> " << i->mod_date.year() << '-';
            if (i->mod_date.month() < 10)
                out << '0';
            out << i->mod_date.month() << '-';
            if (i->mod_date.day() < 10)
                out << '0';
            out << i->mod_date.day() << "</p>\n";

            // view active issues in []
            if (active_issues.count(*i) > 1)
                out << "<p><b>View other</b> " << "<a href=\"" << "lwg-index-open.html#" << remove_square_brackets(i->tags[0])
                    << "\">" << "active issues</a> in " << i->tags[0] << ".</p>\n";

            // view all issues in []
            if (all_issues.count(*i) > 1)
                out << "<p><b>View all other</b> " << "<a href=\"" << "lwg-index.html#" << remove_square_brackets(i->tags[0])
                    << "\">" << "issues</a> in " << i->tags[0] << ".</p>\n";

            // view all issues with same status
            if (issues_by_status.count(*i) > 1)
            {
                out << "<p><b>View all issues with</b> "
                    << "<a href=\"lwg-status.html#" << i->stat << "\">" << i->stat << "</a>"
                    << " status.</p>\n";
            }

            // duplicates
            if (!i->duplicates.empty())
            {
                out << "<p><b>Duplicate of:</b> ";
                out << i->duplicates[0];
                for (unsigned j = 1; j < i->duplicates.size(); ++j)
                    out << ", " << i->duplicates[j];
                out << "</p>\n";
            }

            // text
            out << i->text << "\n\n";
        }
    }
}

void print_paper_heading(std::ostream& out, const std::string& paper)
{
    out << "<table>\n";
    out << "<tr>\n";
    out << "<td align=\"left\">Doc. no.</td>\n";
    out << "<td align=\"left\">" << get_doc_number(paper) << "</td>\n";
    out << "</tr>\n";
    out << "<tr>\n";
    out << "<td align=\"left\">Date:</td>\n";
    out << "<td align=\"left\">" << get_date() << "</td>\n";
    out << "</tr>\n";
    out << "<tr>\n";
    out << "<td align=\"left\">Project:</td>\n";
    out << "<td align=\"left\">Programming Language C++</td>\n";
    out << "</tr>\n";
    out << "<tr>\n";
    out << "<td align=\"left\">Reply to:</td>\n";
    out << "<td align=\"left\">" << get_maintainer() << "</td>\n";
    out << "</tr>\n";
    out << "</table>\n";
    out << "<h1>";
    if (paper == "active")
        out << "C++ Standard Library Active Issues List (Revision ";
    else if (paper == "defect")
        out << "C++ Standard Library Defect Report List (Revision ";
    else if (paper == "closed")
        out << "C++ Standard Library Closed Issues List (Revision ";
    out << get_revision() << ")</h1>\n";
}

void make_active(std::vector<issue>& issues)
{
    sort(issues.begin(), issues.end(), sort_by_num());
    std::ofstream out((path + "lwg-active.html").c_str());
    print_file_header(out, "C++ Standard Library Active Issues List");
    print_paper_heading(out, "active");
    out << get_intro("active") << '\n';
    out << "<h2>Revision History</h2>\n" << get_revisions(issues) << '\n';
    out << "<h2><a name=\"Status\"></a>Issue Status</h2>\n" << get_statuses() << '\n';
    out << "<h2>Active Issues</h2>\n";
    print_issues(out, issues, if_active());   
    print_file_trailer(out);
}

struct if_defect
{
    bool operator()(const issue& i) {return is_defect(i.stat);}
};

void make_defect(std::vector<issue>& issues)
{
    sort(issues.begin(), issues.end(), sort_by_num());
    std::ofstream out((path + "lwg-defects.html").c_str());
    print_file_header(out, "C++ Standard Library Defect Report List");
    print_paper_heading(out, "defect");
    out << get_intro("defect") << '\n';
    out << "<h2>Revision History</h2>\n" << get_revisions(issues) << '\n';
    out << "<h2>Defect Reports</h2>\n";
    print_issues(out, issues, if_defect());   
    print_file_trailer(out);
}

struct if_closed
{
    bool operator()(const issue& i) {return is_closed(i.stat);}
};

void make_closed(std::vector<issue>& issues)
{
    sort(issues.begin(), issues.end(), sort_by_num());
    std::ofstream out((path + "lwg-closed.html").c_str());
    print_file_header(out, "C++ Standard Library Closed Issues List");
    print_paper_heading(out, "closed");
    out << get_intro("closed") << '\n';
    out << "<h2>Revision History</h2>\n" << get_revisions(issues) << '\n';
    out << "<h2>Closed Issues</h2>\n";
    print_issues(out, issues, if_closed());   
    print_file_trailer(out);
}

int parse_month(const std::string& m)
{
    if (m == "Jan")
        return 1;
    if (m == "Feb")
        return 2;
    if (m == "Mar")
        return 3;
    if (m == "Apr")
        return 4;
    if (m == "May")
        return 5;
    if (m == "Jun")
        return 6;
    if (m == "Jul")
        return 7;
    if (m == "Aug")
        return 8;
    if (m == "Sep")
        return 9;
    if (m == "Oct")
        return 10;
    if (m == "Nov")
        return 11;
    if (m == "Dec")
        return 12;
    throw std::runtime_error("unknown month");
}

int
main(int argc, char* argv[])
{
    if (argc == 2)
        path = argv[1];
    else
    {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == 0)
        {
            std::cout << "unable to getcwd\n";
            return 1;
        }
        path = cwd;
    }
    if (path[path.size()-1] != '/')
        path += '/';
    issues_path = path + "issues/";
    section_db = read_section_db();
//    check_against_index(section_db);
    std::vector<issue> issues;
/*    for (unsigned i = 0; i < sizeof(issue_files)/sizeof(issue_files[0]); ++i)
    {
        std::ifstream infile((issues_path + issue_files[i]).c_str());
        if (!infile.is_open())
        {
            std::cout << "Unable to open file " << (issues_path + issue_files[i]) << '\n';
            return 1;
        }
        std::istreambuf_iterator<char> first(infile), last;
        std::string f(first, last);
        infile.close();
        unsigned l2 = 0;
        while (true)
        {
            unsigned k2 = f.find("<sref", l2);
            if (k2 == std::string::npos)
                break;
             l2 = f.find("/>", k2) + 2;
             std::string r = f.substr(k2, l2-k2);
             std::istringstream temp(r);
             std::string w;
             temp >> w; // <sref
             ws(temp);
             char c;
             temp >> c; // r
             temp >> c; // e
             temp >> c; // f
             temp >> c; // =
             temp >> c; // "
             section_num sn;
             temp >> sn;
             if (section_db.find(sn) == section_db.end())
                throw std::runtime_error(issue_files[i] + " invalid section");
             r = "<sref ref=\"" + section_db[sn] + "\"/>";
             f.replace(k2, l2-k2, r);
             l2 = k2 + r.size();
        }
        std::ofstream out((issues_path + issue_files[i]).c_str());
        out << f;
*/
    DIR* dir = opendir(issues_path.c_str());
    if (dir == 0)
    {
        std::cout << "Unable to open issues dir\n";
        return 1;
    }
    for (dirent* entry = readdir(dir); entry != 0; entry = readdir(dir))
    {
        std::string issue_file = entry->d_name;
        if (issue_file.find("issue") != 0)
            continue;
        std::ifstream infile((issues_path + issue_file).c_str());
        if (!infile.is_open())
        {
            std::cout << "Unable to open file " << (issues_path + issue_file) << '\n';
            return 1;
        }
        std::istreambuf_iterator<char> first(infile), last;
        issues.push_back(issue());
        std::string& tx = issues.back().text;
        tx.assign(first, last);
        infile.close();
        
        // Get issue number
        issue& is = issues.back();
        unsigned k = tx.find("<issue num=\"");
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue number in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        k += sizeof("<issue num=\"") - 1;
        unsigned l = tx.find('\"', k);
        std::istringstream temp(tx.substr(k, l-k));
        temp >> is.num;

        // Get issue status
        k = tx.find("status=\"", l);
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue status in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        k += sizeof("status=\"") - 1;
        l = tx.find('\"', k);
        is.stat = tx.substr(k, l-k);

        // Get issue title
        k = tx.find("<title>", l);
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue title in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        k +=  sizeof("<title>") - 1;
        l = tx.find("</title>", k);
        is.title = tx.substr(k, l-k);

        // Get issue sections
        k = tx.find("<section>", l);
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue section in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        k += sizeof("<section>") - 1;
        l = tx.find("</section>", k);
        while (k < l)
        {
            k = tx.find('\"', k);
            if (k >= l)
                break;
            unsigned k2 = tx.find('\"', k+1);
            if (k2 >= l)
            {
                std::cout << "Unable to find issue section in " << (issues_path + issue_file) << '\n';
                return 1;
            }
            ++k;
            is.tags.push_back(tx.substr(k, k2-k));
            if (section_db.find(is.tags.back()) == section_db.end())
            {
                section_num num;
                num.num.push_back(100 + 'X' - 'A');
                section_db[is.tags.back()] = num;
//                 std::cout << "Unable to find issue section: " << is.tags.back() << " in database" << (issues_path + issue_file) << '\n';
//                 return 1;
            }
            k = k2;
            ++k;
        }
        if (is.tags.empty())
        {
            std::cout << "Unable to find issue section in " << (issues_path + issue_file) << '\n';
            return 1;
        }

        // Get submitter
        k = tx.find("<submitter>", l);
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue submitter in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        k += sizeof("<submitter>") - 1;
        l = tx.find("</submitter>", k);
        is.submitter = tx.substr(k, l-k);

        // Get date
        k = tx.find("<date>", l);
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue date in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        k += sizeof("<date>") - 1;
        l = tx.find("</date>", k);
        temp.clear();
        temp.str(tx.substr(k, l-k));
        {
        int d;
        temp >> d;
        if (temp.fail())
            throw std::runtime_error("date format error in " + issue_file);
        std::string month;
        temp >> month;
        int m;
        try
        {
            m = parse_month(month);
            int y = 0;
            temp >> y;
            is.date = greg::month(m)/greg::day(d)/y;
        }
        catch (std::exception& e)
        {
            throw std::runtime_error(e.what() + std::string(" in ") + issue_file);
        }
        }

        // Get modification date
        {
        struct stat buf;
        if (stat((issues_path + issue_file).c_str(), &buf) == -1)
            throw std::runtime_error("call to stat failed for " + issue_file);
        struct tm* mod = std::localtime(&buf.st_mtime);
        is.mod_date = greg::year((unsigned short)(mod->tm_year+1900)) / (mod->tm_mon+1) / mod->tm_mday;
        }

        // Trim text to <discussion>
        k = tx.find("<discussion>", l);
        if (k == std::string::npos)
        {
            std::cout << "Unable to find issue discussion in " << (issues_path + issue_file) << '\n';
            return 1;
        }
        tx.erase(0, k);

        // Find out if issue has a proposed resolution
        if (is_active(is.stat))
        {
            unsigned k2 = tx.find("<resolution>", 0);
            if (k2 == std::string::npos)
            {
                is.has_resolution = false;
            }
            else
            {
                k2 += sizeof("<resolution>") - 1;
                unsigned l2 = tx.find("</resolution>", k2);
                is.has_resolution = l2 - k2 > 15;
            }
        }
        else
            is.has_resolution = true;
    }
    closedir(dir);
    sort(issues.begin(), issues.end(), sort_by_num());
    for (std::vector<issue>::iterator i = issues.begin(), e = issues.end(); i != e; ++i)
        format(issues, *i);
    make_sort_by_num(issues);
    make_sort_by_status(issues);
    make_sort_by_status_mod_date(issues);
    make_sort_by_section(issues);
    make_sort_by_section(issues, true);
    make_active(issues);
    make_defect(issues);
    make_closed(issues);
    std::cout << "Made all documents\n";
}
