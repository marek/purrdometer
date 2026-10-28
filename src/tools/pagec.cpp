// pagec: compiles a .page file (HTML with C++ in <% %> tags) into a C++ function.
//
//   pagec <in.page> <out.cpp> <out.h>
//   pagec --selftest
//
// Both outputs are written beside their destinations and renamed into place;
// any failure is reported and exits non-zero, leaving no half-new pair.
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
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>
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
                s += std::format("\\{:03o}", c);
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
            if (d.starts_with("include "))
                out.includes.push_back(trim(d.substr(8)));
            else if (d.starts_with("param "))
                out.params.push_back(trim(d.substr(6)));
            else
                fail(file, tagLine, "unknown directive: " + d);
        } else if (inner.starts_with("==")) {
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

// A file written in full beside its destination, then renamed over it.
class Staged {
public:
    Staged(const std::string& path, const std::string& content) : path_(path)
    {
        std::ifstream old(path, std::ios::binary);
        if (old) {
            std::stringstream current;
            current << old.rdbuf();
            if (current.str() == content)
                return; // unchanged output keeps its timestamp: rebuilding pagec does not rebuild its includers
        }
        tmp_ = path + ".tmp." + std::to_string(getpid());
        std::ofstream out(tmp_, std::ios::binary | std::ios::trunc);
        out << content;
        out.flush();
        if (!out) {
            std::string why = std::strerror(errno);
            out.close();
            std::remove(tmp_.c_str());
            throw std::runtime_error("cannot write " + tmp_ + ": " + why);
        }
    }
    ~Staged()
    {
        if (!tmp_.empty())
            std::remove(tmp_.c_str());
    }
    Staged(const Staged&) = delete;
    Staged& operator=(const Staged&) = delete;

    bool pending() const { return !tmp_.empty(); }
    void publish()
    {
        if (tmp_.empty())
            return;
        if (std::rename(tmp_.c_str(), path_.c_str()) != 0)
            throw std::runtime_error("cannot replace " + path_ + ": " + std::strerror(errno));
        tmp_.clear();
        published_ = true;
    }
    void withdraw() // a published file the other half failed to follow: gone, so make builds it again
    {
        if (published_)
            unlink(path_.c_str());
    }

private:
    std::string path_, tmp_;
    bool published_ = false;
};

// Both outputs or neither: a failure leaves no half-new pair for the build to use.
int generate(const std::string& in, const std::string& cppPath, const std::string& hPath)
{
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
    std::string hname = hPath.substr(hPath.find_last_of('/') + 1);

    std::string source = "// Generated by pagec from " + in + ". Do not edit.\n#include \"" + hname +
                         "\"\n#include \"web/html.h\"\n\nnamespace purr::pages {\n\nvoid " + name + "(" + params +
                         ")\n{\n" + t.body + "}\n\n} // namespace purr::pages\n";
    try {
        Staged h(hPath, header), cpp(cppPath, source);
        h.publish();
        try {
            cpp.publish();
        } catch (...) {
            h.withdraw();
            throw;
        }
    } catch (const std::exception& e) {
        std::cerr << "pagec: " << in << ": " << e.what() << "\n";
        return 1;
    }
    return 0;
}

int selftest()
{
    int failures = 0;
    auto check = [&](bool ok, const std::string& what) {
        std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
        failures += !ok;
    };
    namespace fs = std::filesystem;
    const char* tmpEnv = std::getenv("PURR_TMP");
    fs::path dir = fs::path(tmpEnv ? tmpEnv : "tmp") / "pagec-selftest";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "hello.page") << "<%@ param int n %>\n<p><%= n %></p>\n";
    std::string in = (dir / "hello.page").string(), cpp = (dir / "out.cpp").string(), h = (dir / "out.h").string();
    auto text = [](const std::string& path) {
        std::ifstream f(path);
        return std::string(std::istreambuf_iterator<char>(f), {});
    };
    auto leftovers = [&] {
        int n = 0;
        for (auto& e : fs::directory_iterator(dir))
            n += e.path().string().find(".tmp.") != std::string::npos;
        return n;
    };

    check(generate(in, cpp, h) == 0 && text(h).contains("void hello(std::string& out, int n);") &&
              text(cpp).contains("purr::web::text(out, n)"),
          "a page becomes a function and its header");
    auto stamp = fs::last_write_time(cpp);
    check(generate(in, cpp, h) == 0 && fs::last_write_time(cpp) == stamp, "unchanged output keeps its timestamp");

    std::ofstream(dir / "hello.page") << "<%@ param long n %>\n<p><%= n + 1 %></p>\n";
    fs::create_directory(dir / "blocked.cpp");
    check(generate(in, (dir / "blocked.cpp").string(), h) == 1 && !fs::exists(h) && leftovers() == 0,
          "a source that cannot be replaced fails, and takes back the header it was paired with");
    fs::remove(dir / "blocked.cpp");
    fs::remove(h);
    fs::create_directory(h);
    check(generate(in, cpp, h) == 1 && text(cpp).contains("purr::web::text(out, n)") && !text(cpp).contains("n + 1") &&
              leftovers() == 0,
          "a header that cannot be replaced fails before the source changes");
    fs::remove(h);
    check(generate(in, (dir / "missing" / "out.cpp").string(), h) == 1 && !fs::exists(h) && leftovers() == 0,
          "a missing destination folder fails and writes nothing");
    check(generate((dir / "nothing.page").string(), cpp, h) == 1, "a missing page fails");
    check(generate(in, cpp, h) == 0 && text(cpp).contains("n + 1"), "once the destination is writable the output follows");

    fs::remove_all(dir);
    std::cout << (failures ? "FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--selftest")
        return selftest();
    if (argc != 4) {
        std::cerr << "usage: pagec <in.page> <out.cpp> <out.h> | pagec --selftest\n";
        return 2;
    }
    return generate(argv[1], argv[2], argv[3]);
}
