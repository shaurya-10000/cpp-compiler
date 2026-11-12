#pragma once
// Text-level preprocessor: #include "file", #define (object- and function-like), #undef,
// #if/#ifdef/#ifndef/#elif/#else/#endif, #error, #pragma once, __LINE__ and __FILE__.
// Every directive or skipped line still yields one (blank) output line, and an #include is replaced
// by the included file's lines. origin() maps an output line back to (file, line) for diagnostics.
// Not supported: '#' / '##' in macro bodies, variadic macros, macro calls spanning several lines.
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Preprocessor {
    struct Macro {
        bool is_func = false;
        std::vector<std::string> params;
        std::string body;
    };
    struct Cond {
        bool parent = true;      // the enclosing region is active
        bool taken = false;      // some branch of this #if chain was already taken
        bool active = true;      // the current branch is being emitted
        bool seen_else = false;
    };

public:
    struct Origin {
        int file = 0;
        int line = 0;
    };

    void add_include_dir(const std::string& dir) { include_dirs_.push_back(dir); }
    void define(const std::string& name, const std::string& body) {
        Macro m;
        m.body = body;
        macros_[name] = m;
    }

    std::string run(const std::string& name, const std::string& text) {
        out_.clear();
        origins_.clear();
        file_names_.clear();
        file_lines_.clear();
        conds_.clear();
        once_.clear();
        depth_ = 0;
        cond_base_ = 0;
        in_comment_ = false;
        cur_file_ = 0;
        cur_line_ = 0;
        process_file(name, text);
        const bool trailing = !text.empty() && text.back() == '\n';
        std::string result;
        for (size_t i = 0; i < out_.size(); ++i) {
            result += out_[i];
            if (i + 1 < out_.size() || trailing) result += '\n';
        }
        return result;
    }

    const std::string& file_name(int f) const { return file_names_[static_cast<size_t>(f)]; }
    std::string file_text(int f) const {
        std::string t;
        for (const auto& l : file_lines_[static_cast<size_t>(f)]) { t += l; t += '\n'; }
        return t;
    }
    // 1-based line of the preprocessed text -> originating file and line.
    bool origin(int out_line, Origin& o) const {
        if (out_line < 1 || out_line > static_cast<int>(origins_.size())) return false;
        o = origins_[static_cast<size_t>(out_line - 1)];
        return true;
    }

private:
    std::unordered_map<std::string, Macro> macros_;
    std::vector<Cond> conds_;
    std::vector<std::string> out_;
    std::vector<Origin> origins_;
    std::vector<std::string> file_names_;
    std::vector<std::vector<std::string>> file_lines_;
    std::unordered_set<std::string> once_;
    std::vector<std::string> include_dirs_;
    int cur_file_ = 0;
    int cur_line_ = 0;
    int depth_ = 0;
    size_t cond_base_ = 0;
    bool in_comment_ = false;

    // ---- small helpers ----
    static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }
    static bool is_digit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
    static bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }
    static bool is_ident(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

    static std::string trim(const std::string& s) {
        size_t b = 0, e = s.size();
        while (b < e && is_space(s[b])) ++b;
        while (e > b && is_space(s[e - 1])) --e;
        return s.substr(b, e - b);
    }

    static std::vector<std::string> split_lines(const std::string& text) {
        std::vector<std::string> lines;
        std::string cur;
        for (char c : text) {
            if (c == '\n') { lines.push_back(cur); cur.clear(); }
            else if (c != '\r') cur += c;
        }
        if (!cur.empty()) lines.push_back(cur);
        return lines;
    }

    // Index of the closing quote of the literal starting at s[i] (or the last index if unterminated).
    static size_t copy_quoted(const std::string& s, size_t i) {
        const char q = s[i];
        size_t j = i + 1;
        while (j < s.size() && s[j] != q) {
            if (s[j] == '\\' && j + 1 < s.size()) ++j;
            ++j;
        }
        return j < s.size() ? j : s.size() - 1;
    }

    static std::string strip_comments(const std::string& s) {
        std::string out;
        size_t i = 0;
        const size_t n = s.size();
        while (i < n) {
            const char c = s[i];
            if (c == '/' && i + 1 < n && s[i + 1] == '/') break;
            if (c == '/' && i + 1 < n && s[i + 1] == '*') {
                const size_t e = s.find("*/", i + 2);
                if (e == std::string::npos) break;
                out += ' ';
                i = e + 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                const size_t e = copy_quoted(s, i);
                out += s.substr(i, e - i + 1);
                i = e + 1;
                continue;
            }
            out += c;
            ++i;
        }
        return out;
    }

    static bool is_directive(const std::string& line) {
        for (char c : line) {
            if (is_space(c)) continue;
            return c == '#';
        }
        return false;
    }

    [[noreturn]] void fail(const std::string& msg) {
        origins_.push_back(Origin{cur_file_, cur_line_});
        throw std::runtime_error("Preprocess error line " + std::to_string(origins_.size()) + ": " + msg);
    }

    void emit(const std::string& text) {
        out_.push_back(text);
        origins_.push_back(Origin{cur_file_, cur_line_});
    }

    // Tracks /* */ state across lines for text that is not being macro-expanded.
    void scan_comments(const std::string& s) {
        size_t i = 0;
        const size_t n = s.size();
        while (i < n) {
            if (in_comment_) {
                const size_t e = s.find("*/", i);
                if (e == std::string::npos) return;
                in_comment_ = false;
                i = e + 2;
                continue;
            }
            const char c = s[i];
            if (c == '/' && i + 1 < n && s[i + 1] == '/') return;
            if (c == '/' && i + 1 < n && s[i + 1] == '*') { in_comment_ = true; i += 2; continue; }
            if (c == '"' || c == '\'') { i = copy_quoted(s, i) + 1; continue; }
            ++i;
        }
    }

    // ---- file / line driver ----
    void process_file(const std::string& name, const std::string& text) {
        if (depth_ >= 64) fail("#include nested too deeply");
        ++depth_;
        const int file = static_cast<int>(file_names_.size());
        file_names_.push_back(name);
        file_lines_.push_back(split_lines(text));
        const std::vector<std::string> lines = file_lines_[static_cast<size_t>(file)];
        const size_t saved_base = cond_base_;
        const bool saved_comment = in_comment_;
        cond_base_ = conds_.size();
        in_comment_ = false;

        for (size_t i = 0; i < lines.size(); ++i) {
            cur_file_ = file;
            cur_line_ = static_cast<int>(i) + 1;
            std::string line = lines[i];

            if (!in_comment_ && is_directive(line)) {
                size_t extra = 0;
                while (!line.empty() && line.back() == '\\' && i + 1 < lines.size()) {
                    line.pop_back();
                    ++i;
                    ++extra;
                    line += lines[i];
                }
                const int dir_line = cur_line_;
                directive(line, file);
                cur_file_ = file;
                cur_line_ = dir_line;
                emit("");
                for (size_t k = 0; k < extra; ++k) {
                    cur_line_ = dir_line + 1 + static_cast<int>(k);
                    emit("");
                }
                continue;
            }
            if (!conds_.empty() && !conds_.back().active) {
                scan_comments(line);
                emit("");
                continue;
            }
            std::unordered_set<std::string> active;
            emit(expand(line, active, true));
        }

        if (conds_.size() != cond_base_) {
            cur_file_ = file;
            cur_line_ = static_cast<int>(lines.size());
            fail("unterminated conditional directive (missing #endif)");
        }
        cond_base_ = saved_base;
        in_comment_ = saved_comment;
        --depth_;
    }

    // ---- directives ----
    void directive(const std::string& raw, int file) {
        namespace fs = std::filesystem;
        const std::string line = strip_comments(raw);
        size_t p = line.find('#') + 1;
        while (p < line.size() && is_space(line[p])) ++p;
        size_t q = p;
        while (q < line.size() && is_ident(line[q])) ++q;
        const std::string d = line.substr(p, q - p);
        const std::string rest = trim(line.substr(q));
        const bool active = conds_.empty() || conds_.back().active;

        if (d == "if" || d == "ifdef" || d == "ifndef") {
            bool v = false;
            if (active) {
                if (d == "if") {
                    v = eval_if(rest);
                } else {
                    size_t e = 0;
                    while (e < rest.size() && is_ident(rest[e])) ++e;
                    const std::string name = rest.substr(0, e);
                    if (name.empty()) fail("macro name missing after #" + d);
                    const bool def = macros_.count(name) != 0;
                    v = (d == "ifdef") ? def : !def;
                }
            }
            Cond c;
            c.parent = active;
            c.active = v;
            c.taken = v;
            conds_.push_back(c);
            return;
        }
        if (d == "elif" || d == "else") {
            if (conds_.size() <= cond_base_) fail("#" + d + " without #if");
            Cond& c = conds_.back();
            if (c.seen_else) fail("#" + d + " after #else");
            if (d == "else") {
                c.seen_else = true;
                c.active = c.parent && !c.taken;
                if (c.active) c.taken = true;
            } else if (!c.parent || c.taken) {
                c.active = false;
            } else {
                const bool v = eval_if(rest);
                c.active = v;
                c.taken = v;
            }
            return;
        }
        if (d == "endif") {
            if (conds_.size() <= cond_base_) fail("#endif without #if");
            conds_.pop_back();
            return;
        }
        if (!active) return;

        if (d == "define") {
            size_t e = 0;
            while (e < rest.size() && is_ident(rest[e])) ++e;
            const std::string name = rest.substr(0, e);
            if (name.empty() || is_digit(name[0])) fail("macro name missing in #define");
            if (name == "defined" || name == "__LINE__" || name == "__FILE__")
                fail("cannot redefine '" + name + "'");
            Macro m;
            if (e < rest.size() && rest[e] == '(') {
                m.is_func = true;
                ++e;
                for (;;) {
                    while (e < rest.size() && is_space(rest[e])) ++e;
                    if (e < rest.size() && rest[e] == ')') { ++e; break; }
                    size_t k = e;
                    while (k < rest.size() && is_ident(rest[k])) ++k;
                    if (k == e) fail("expected a parameter name in #define " + name);
                    m.params.push_back(rest.substr(e, k - e));
                    e = k;
                    while (e < rest.size() && is_space(rest[e])) ++e;
                    if (e < rest.size() && rest[e] == ',') { ++e; continue; }
                    if (e < rest.size() && rest[e] == ')') { ++e; break; }
                    fail("expected ',' or ')' in the parameter list of #define " + name);
                }
            }
            m.body = trim(rest.substr(e));
            auto it = macros_.find(name);
            if (it != macros_.end() && (it->second.is_func != m.is_func ||
                                        it->second.params != m.params || it->second.body != m.body))
                fail("redefinition of macro '" + name + "'");
            macros_[name] = m;
            return;
        }
        if (d == "undef") {
            size_t e = 0;
            while (e < rest.size() && is_ident(rest[e])) ++e;
            const std::string name = rest.substr(0, e);
            if (name.empty()) fail("macro name missing in #undef");
            macros_.erase(name);
            return;
        }
        if (d == "include") {
            if (rest.empty() || rest[0] != '"') {
                if (!rest.empty() && rest[0] == '<')
                    fail("system headers are not available (minicc has no libc); use #include \"file\"");
                fail("expected \"file\" after #include");
            }
            const size_t e = rest.find('"', 1);
            if (e == std::string::npos) fail("unterminated file name in #include");
            const std::string rel = rest.substr(1, e - 1);
            std::vector<fs::path> candidates;
            candidates.push_back(fs::path(file_names_[static_cast<size_t>(file)]).parent_path() / rel);
            for (const auto& dir : include_dirs_) candidates.push_back(fs::path(dir) / rel);
            fs::path found;
            for (const auto& c : candidates) {
                std::error_code ec;
                if (fs::is_regular_file(c, ec)) { found = c; break; }
            }
            if (found.empty()) fail("cannot open include file '" + rel + "'");
            std::error_code ec;
            const std::string canon = fs::weakly_canonical(found, ec).string();
            if (once_.count(canon) != 0) return;
            std::ifstream in(found, std::ios::binary);
            if (!in) fail("cannot open include file '" + rel + "'");
            std::stringstream ss;
            ss << in.rdbuf();
            process_file(found.string(), ss.str());
            return;
        }
        if (d == "error") fail("#error " + rest);
        if (d == "pragma") {
            if (rest == "once") {
                std::error_code ec;
                once_.insert(fs::weakly_canonical(fs::path(file_names_[static_cast<size_t>(file)]), ec).string());
            }
            return;
        }
        if (d.empty()) return;   // null directive
        fail("unknown preprocessor directive '#" + d + "'");
    }

    // ---- #if expressions ----
    struct IfEval {
        const std::string& s;
        size_t p = 0;
        bool bad = false;
        explicit IfEval(const std::string& str) : s(str) {}

        void ws() { while (p < s.size() && is_space(s[p])) ++p; }

        long long primary() {
            ws();
            if (p >= s.size()) { bad = true; return 0; }
            const char c = s[p];
            if (c == '(') {
                ++p;
                const long long v = ternary();
                ws();
                if (p < s.size() && s[p] == ')') ++p; else bad = true;
                return v;
            }
            if (c == '!') { ++p; return primary() == 0 ? 1 : 0; }
            if (c == '~') { ++p; return ~primary(); }
            if (c == '-') { ++p; return static_cast<long long>(0ULL - static_cast<unsigned long long>(primary())); }
            if (c == '+') { ++p; return primary(); }
            if (is_digit(c)) {
                long long v = 0;
                if (c == '0' && p + 1 < s.size() && (s[p + 1] == 'x' || s[p + 1] == 'X')) {
                    p += 2;
                    while (p < s.size() && std::isxdigit(static_cast<unsigned char>(s[p])) != 0) {
                        const char h = s[p];
                        v = v * 16 + (is_digit(h) ? h - '0' : std::tolower(static_cast<unsigned char>(h)) - 'a' + 10);
                        ++p;
                    }
                } else {
                    while (p < s.size() && is_digit(s[p])) { v = v * 10 + (s[p] - '0'); ++p; }
                }
                while (p < s.size() && (s[p] == 'u' || s[p] == 'U' || s[p] == 'l' || s[p] == 'L')) ++p;
                return v;
            }
            if (c == '\'') {
                ++p;
                if (p >= s.size()) { bad = true; return 0; }
                char ch = s[p++];
                if (ch == '\\' && p < s.size()) {
                    const char e = s[p++];
                    ch = e == 'n' ? '\n' : e == 't' ? '\t' : e == '0' ? '\0' : e;
                }
                if (p < s.size() && s[p] == '\'') ++p; else bad = true;
                return static_cast<signed char>(ch);
            }
            if (is_ident_start(c)) {   // an identifier that is not a macro evaluates to 0
                while (p < s.size() && is_ident(s[p])) ++p;
                return 0;
            }
            bad = true;
            return 0;
        }

        long long apply(const std::string& op, long long l, long long r) {
            using U = unsigned long long;
            if (op == "||") return (l != 0 || r != 0) ? 1 : 0;
            if (op == "&&") return (l != 0 && r != 0) ? 1 : 0;
            if (op == "|") return l | r;
            if (op == "^") return l ^ r;
            if (op == "&") return l & r;
            if (op == "==") return l == r ? 1 : 0;
            if (op == "!=") return l != r ? 1 : 0;
            if (op == "<=") return l <= r ? 1 : 0;
            if (op == ">=") return l >= r ? 1 : 0;
            if (op == "<") return l < r ? 1 : 0;
            if (op == ">") return l > r ? 1 : 0;
            if (op == "<<") return static_cast<long long>(static_cast<U>(l) << (static_cast<U>(r) & 63U));
            if (op == ">>") return l >> static_cast<int>(static_cast<U>(r) & 63U);
            if (op == "+") return static_cast<long long>(static_cast<U>(l) + static_cast<U>(r));
            if (op == "-") return static_cast<long long>(static_cast<U>(l) - static_cast<U>(r));
            if (op == "*") return static_cast<long long>(static_cast<U>(l) * static_cast<U>(r));
            if (r == 0) { bad = true; return 0; }
            if (r == -1) return static_cast<long long>(0ULL - static_cast<U>(l)) * (op == "/" ? 1 : 0);
            return op == "/" ? l / r : l % r;
        }

        long long binary(size_t lvl) {
            static const std::vector<std::vector<std::string>> levels = {
                {"||"}, {"&&"}, {"|"}, {"^"}, {"&"}, {"==", "!="}, {"<=", ">=", "<", ">"},
                {"<<", ">>"}, {"+", "-"}, {"*", "/", "%"},
            };
            if (lvl == levels.size()) return primary();
            long long l = binary(lvl + 1);
            for (;;) {
                ws();
                std::string found;
                for (const auto& op : levels[lvl])
                    if (s.compare(p, op.size(), op) == 0) { found = op; break; }
                if (found.empty()) return l;
                if ((found == "|" || found == "&") && p + 1 < s.size() && s[p + 1] == found[0]) return l;
                p += found.size();
                const long long r = binary(lvl + 1);
                l = apply(found, l, r);
            }
        }

        long long ternary() {
            const long long c = binary(0);
            ws();
            if (p < s.size() && s[p] == '?') {
                ++p;
                const long long a = ternary();
                ws();
                if (p < s.size() && s[p] == ':') ++p; else bad = true;
                const long long b = ternary();
                return c != 0 ? a : b;
            }
            return c;
        }
    };

    std::string replace_defined(const std::string& s) {
        std::string out;
        size_t i = 0;
        while (i < s.size()) {
            if (is_ident_start(s[i])) {
                size_t j = i;
                while (j < s.size() && is_ident(s[j])) ++j;
                const std::string id = s.substr(i, j - i);
                if (id == "defined") {
                    size_t k = j;
                    while (k < s.size() && is_space(s[k])) ++k;
                    const bool paren = k < s.size() && s[k] == '(';
                    if (paren) {
                        ++k;
                        while (k < s.size() && is_space(s[k])) ++k;
                    }
                    size_t m = k;
                    while (m < s.size() && is_ident(s[m])) ++m;
                    const std::string name = s.substr(k, m - k);
                    if (name.empty()) fail("'defined' requires a macro name");
                    if (paren) {
                        while (m < s.size() && is_space(s[m])) ++m;
                        if (m < s.size() && s[m] == ')') ++m;
                        else fail("missing ')' after 'defined('");
                    }
                    out += macros_.count(name) != 0 ? "1" : "0";
                    i = m;
                    continue;
                }
                out += id;
                i = j;
                continue;
            }
            out += s[i++];
        }
        return out;
    }

    bool eval_if(const std::string& expr) {
        if (trim(expr).empty()) fail("#if/#elif with no expression");
        std::string e = replace_defined(expr);
        std::unordered_set<std::string> act;
        e = expand(e, act, false);
        IfEval ev(e);
        const long long v = ev.ternary();
        ev.ws();
        if (ev.bad || ev.p != e.size()) fail("invalid expression in #if/#elif: '" + trim(expr) + "'");
        return v != 0;
    }

    // ---- macro expansion ----
    std::string substitute(const Macro& m, const std::vector<std::string>& args) const {
        std::string out;
        const std::string& b = m.body;
        size_t i = 0;
        const size_t n = b.size();
        while (i < n) {
            const char c = b[i];
            if (c == '"' || c == '\'') {
                const size_t e = copy_quoted(b, i);
                out += b.substr(i, e - i + 1);
                i = e + 1;
                continue;
            }
            if (is_ident_start(c)) {
                size_t j = i;
                while (j < n && is_ident(b[j])) ++j;
                const std::string id = b.substr(i, j - i);
                bool replaced = false;
                for (size_t k = 0; k < m.params.size(); ++k) {
                    if (m.params[k] == id) { out += args[k]; replaced = true; break; }
                }
                if (!replaced) out += id;
                i = j;
                continue;
            }
            if (is_digit(c)) {
                size_t j = i;
                while (j < n && (is_ident(b[j]) || b[j] == '.')) ++j;
                out += b.substr(i, j - i);
                i = j;
                continue;
            }
            out += c;
            ++i;
        }
        return out;
    }

    // `top` is true for a whole source line (it carries the /* */ state across lines).
    std::string expand(const std::string& s, std::unordered_set<std::string>& active, bool top) {
        std::string out;
        size_t i = 0;
        const size_t n = s.size();
        while (i < n) {
            const char c = s[i];
            if (top && in_comment_) {
                const size_t e = s.find("*/", i);
                if (e == std::string::npos) { out += s.substr(i); i = n; }
                else { out += s.substr(i, e + 2 - i); i = e + 2; in_comment_ = false; }
                continue;
            }
            if (c == '/' && i + 1 < n && s[i + 1] == '/') { out += s.substr(i); break; }
            if (c == '/' && i + 1 < n && s[i + 1] == '*') {
                const size_t e = s.find("*/", i + 2);
                if (e == std::string::npos) {
                    out += s.substr(i);
                    if (top) in_comment_ = true;
                    break;
                }
                out += s.substr(i, e + 2 - i);
                i = e + 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                const size_t e = copy_quoted(s, i);
                out += s.substr(i, e - i + 1);
                i = e + 1;
                continue;
            }
            if (is_digit(c)) {
                size_t j = i;
                while (j < n && (is_ident(s[j]) || s[j] == '.')) ++j;
                out += s.substr(i, j - i);
                i = j;
                continue;
            }
            if (!is_ident_start(c)) { out += c; ++i; continue; }

            size_t j = i;
            while (j < n && is_ident(s[j])) ++j;
            const std::string id = s.substr(i, j - i);
            if (id == "__LINE__") { out += std::to_string(cur_line_); i = j; continue; }
            if (id == "__FILE__") {
                std::string f;
                for (char ch : file_names_[static_cast<size_t>(cur_file_)]) {
                    if (ch == '\\' || ch == '"') f += '\\';
                    f += ch;
                }
                out += "\"" + f + "\"";
                i = j;
                continue;
            }
            auto it = macros_.find(id);
            if (it == macros_.end() || active.count(id) != 0) { out += id; i = j; continue; }
            const Macro& m = it->second;
            if (!m.is_func) {
                active.insert(id);
                out += expand(m.body, active, false);
                active.erase(id);
                i = j;
                continue;
            }
            size_t k = j;
            while (k < n && (s[k] == ' ' || s[k] == '\t')) ++k;
            if (k >= n || s[k] != '(') { out += id; i = j; continue; }

            std::vector<std::string> args;
            std::string cur;
            int depth = 0;
            size_t pp = k + 1;
            bool closed = false;
            for (; pp < n; ++pp) {
                const char d = s[pp];
                if (d == '"' || d == '\'') {
                    const size_t e = copy_quoted(s, pp);
                    cur += s.substr(pp, e - pp + 1);
                    pp = e;
                    continue;
                }
                if (d == '(') { ++depth; cur += d; }
                else if (d == ')') {
                    if (depth == 0) { closed = true; break; }
                    --depth;
                    cur += d;
                } else if (d == ',' && depth == 0) { args.push_back(cur); cur.clear(); }
                else cur += d;
            }
            if (!closed) fail("unterminated argument list invoking macro '" + id + "'");
            args.push_back(cur);
            if (m.params.empty() && args.size() == 1 && trim(args[0]).empty()) args.clear();
            if (args.size() != m.params.size())
                fail("macro '" + id + "' expects " + std::to_string(m.params.size()) +
                     (m.params.size() == 1 ? " argument" : " arguments") + ", got " +
                     std::to_string(args.size()));
            for (auto& a : args) a = expand(trim(a), active, false);
            const std::string body = substitute(m, args);
            active.insert(id);
            out += expand(body, active, false);
            active.erase(id);
            i = pp + 1;
        }
        return out;
    }
};
