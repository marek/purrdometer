// pagec: compiles a .page file (HTML with C++ in <% %> tags) into a C++ function.
//
//   pagec <in.page> <out.cpp> <out.h>
//
// The template "dashboard.page" becomes
//   void purr::pages::dashboard(std::string& out, <params>);
//
// Tags:
//   <%@ include "path.h" %>   #include in the generated files
//   <%@ param <decl> %>        a function parameter (repeatable, in order)
//   <% code %>                 C++ statements
//   <%= expr %>                expr, HTML-escaped (purr::web::text)
//   <%== expr %>               expr, raw (purr::web::raw)
//   <%# comment %>             dropped
//   -%>                        ends any tag and drops the newline after it
//
// #line directives point compiler errors at the template.

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Out {
    std::vector<std::string> includes, params;
    std::string body;
};

[[noreturn]] void fail(const std::string& file, int line, const std::string& msg)
{
    std::cerr << file << ":" << line << ": " << msg << "\n";
    std::exit(1);
}

std::string trim(const std::string& s)
{
    auto b = s.find_first_not_of(" \t\r\n");
    auto e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

std::string quote(const std::string& file) // for #line
{
    std::string q = "\"";
    for (char c : file)
        q += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
    return q + "\"";
}

std::string literal(const std::string& text)
{
    std::string s = "\"";
    for (unsigned char c : text) {
        switch (c) {
        case '"': s += "\\\""; break;
        case '\\': s += "\\\\"; break;
        case '\n': s += "\\n\"\n        \""; break;
        case '\t': s += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\%03o", c);
                s += buf;
            } else {
                s += char(c);
            }
        }
    }
    return s + "\"";
}

int linesIn(const std::string& s, size_t from, size_t to)
{
    int n = 0;
    for (size_t i = from; i < to && i < s.size(); ++i)
        n += s[i] == '\n';
    return n;
}

Out compile(const std::string& file, const std::string& src)
{
    Out out;
    std::string lineFile = quote(file);
    size_t pos = 0;
    int line = 1;
    while (pos < src.size()) {
        size_t open = src.find("<%", pos);
        std::string text = src.substr(pos, open == std::string::npos ? std::string::npos : open - pos);
        if (!text.empty())
            out.body += "    out.append(" + literal(text) + ");\n";
        line += linesIn(src, pos, open == std::string::npos ? src.size() : open);
        if (open == std::string::npos)
            break;

        size_t close = src.find("%>", open + 2);
        if (close == std::string::npos)
            fail(file, line, "unterminated <%");
        bool trimNewline = close > open + 2 && src[close - 1] == '-';
        std::string inner = src.substr(open + 2, close - open - 2 - (trimNewline ? 1 : 0));
        int tagLine = line;
        line += linesIn(src, open, close);
        pos = close + 2;
        if (trimNewline && pos < src.size() && src[pos] == '\n') {
            ++pos;
            ++line;
        }

        std::string at = "#line " + std::to_string(tagLine) + " " + lineFile + "\n";
        if (!inner.empty() && inner[0] == '#')
            continue;
        if (!inner.empty() && inner[0] == '@') {
            std::string d = trim(inner.substr(1));
            if (d.rfind("include ", 0) == 0)
                out.includes.push_back(trim(d.substr(8)));
            else if (d.rfind("param ", 0) == 0)
                out.params.push_back(trim(d.substr(6)));
            else
                fail(file, tagLine, "unknown directive: " + d);
        } else if (inner.rfind("==", 0) == 0) {
            out.body += at + "    purr::web::raw(out, " + trim(inner.substr(2)) + ");\n";
        } else if (!inner.empty() && inner[0] == '=') {
            out.body += at + "    purr::web::text(out, " + trim(inner.substr(1)) + ");\n";
        } else {
            out.body += at + "    " + trim(inner) + "\n";
        }
    }
    return out;
}

std::string functionName(const std::string& path)
{
    auto slash = path.find_last_of('/');
    std::string base = path.substr(slash == std::string::npos ? 0 : slash + 1);
    base = base.substr(0, base.find('.'));
    for (char& c : base)
        if (!std::isalnum(static_cast<unsigned char>(c)))
            c = '_';
    return base;
}

void writeIfChanged(const std::string& path, const std::string& content)
{
    std::ifstream old(path);
    std::stringstream current;
    current << old.rdbuf();
    if (old && current.str() == content)
        return;
    std::ofstream(path) << content;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "usage: pagec <in.page> <out.cpp> <out.h>\n";
        return 2;
    }
    std::string in = argv[1];
    std::ifstream f(in);
    if (!f) {
        std::cerr << "pagec: cannot read " << in << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    Out t = compile(in, ss.str());

    std::string name = functionName(in), params = "std::string& out";
    for (auto& p : t.params)
        params += ", " + p;
    std::string includes;
    for (auto& i : t.includes)
        includes += "#include " + i + "\n";
    std::string header = "// Generated by pagec from " + in + ". Do not edit.\n#pragma once\n\n#include <string>\n" +
                         includes + "\nnamespace purr::pages {\nvoid " + name + "(" + params + ");\n}\n";
    std::string hpath = argv[3];
    std::string hname = hpath.substr(hpath.find_last_of('/') + 1);

    std::string source = "// Generated by pagec from " + in + ". Do not edit.\n#include \"" + hname +
                         "\"\n#include \"web/html.h\"\n\nnamespace purr::pages {\n\nvoid " + name + "(" + params +
                         ")\n{\n" + t.body + "}\n\n} // namespace purr::pages\n";
    // Unchanged output keeps its timestamp, so rebuilding pagec does not rebuild everything that includes it.
    writeIfChanged(argv[3], header);
    writeIfChanged(argv[2], source);
    return 0;
}
