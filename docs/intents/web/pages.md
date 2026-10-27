# Pages

> Owns how web pages are written: `.page` files compiled into C++ by
> `pagec`. What the pages show is owned by [dashboard.md](dashboard.md).

## Requirement

Writing a page is writing HTML with C++ in it, in the manner of Drogon's C++
Server Pages: any C++ function or value can be used in place, nothing is
interpreted at runtime, and a mistake in a page is a compile error that points
at the page's line. `.page` and `pagec` are this project's own; the `<% %>`
tags are the common ones of JSP, ASP and EJS.

## Files

| Subject | Rule |
|---|---|
| Source | `web/pages/<name>.page` |
| Compiler | `pagec` (`src/tools/pagec.cpp`), built first by `make` |
| Output | `build/gen/<name>.page.cpp` and `.page.h`, never committed |
| Function | `void purr::pages::<name>(std::string& out, <params>)`, appending the page to `out` |
| Data | A page receives a view struct built in C++ (for the dashboard, `purr::Dashboard` from `buildDashboard`); arithmetic for drawing stays in the page, deciding what to show stays in C++ |

## Tags

| Tag | Meaning |
|---|---|
| `<%@ include "path.h" %>` | An `#include` in the generated files |
| `<%@ param <declaration> %>` | A parameter of the generated function, in order |
| `<% code %>` | C++ statements |
| `<%= expr %>` | `expr`, HTML-escaped |
| `<%== expr %>` | `expr`, unescaped |
| `<%# comment %>` | Dropped |
| `-%>` | Ends any tag and drops the newline after it |

## Output rules

| Subject | Rule |
|---|---|
| Escaping | `<%= %>` escapes `& < > " '`; it is the default, and `<%== %>` is for markup a C++ function built on purpose |
| Types | `<%= %>` takes text and integers. A double, a date or a distance goes through a named formatter (`purr::web::km`, `fixed`, `thousands`, `ago`, `cal::longDate`), so every format is chosen on purpose |
| Errors | `#line` directives map compiler errors to `<page>:<line>` |
| Cost | A page is string appends; no parsing, no allocation per tag beyond the output |
