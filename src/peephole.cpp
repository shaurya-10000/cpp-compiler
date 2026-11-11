#include "peephole.h"

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
bool ends_with(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}
bool contains(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

// Instruction lines are indented by two spaces and do not start with '.' (data directives).
bool is_instr(const std::string& l) {
    return l.size() > 3 && l[0] == ' ' && l[1] == ' ' && l[2] != '.' && l[2] != ' ';
}

std::string mnemonic(const std::string& l) {
    const size_t e = l.find(' ', 2);
    return l.substr(2, e == std::string::npos ? std::string::npos : e - 2);
}

// Straight-line instructions that never touch control flow or the stack pointer implicitly.
bool is_safe_mnemonic(const std::string& m) {
    static const std::unordered_set<std::string> ok = {
        "mov", "movabsq", "movsbq", "movzbq", "movq", "lea", "add", "sub", "imul", "neg", "not",
        "and", "or", "xor", "shl", "sar", "cmp", "test", "sete", "setne", "setl", "setg", "setle",
        "setge", "setb", "seta", "setbe", "setae", "setp", "setnp", "cvtsi2sdq", "cvttsd2si",
        "addsd", "subsd", "mulsd", "divsd", "ucomisd", "xorpd",
    };
    return ok.count(m) != 0;
}

const std::vector<std::string>* family(const std::string& reg) {
    static const std::unordered_map<std::string, std::vector<std::string>> fam = {
        {"%rcx", {"%rcx", "%ecx", "%cx", "%cl", "%ch"}},
        {"%rdx", {"%rdx", "%edx", "%dx", "%dl", "%dh"}},
        {"%rdi", {"%rdi", "%edi", "%di"}},
        {"%rsi", {"%rsi", "%esi", "%si"}},
        {"%r8", {"%r8"}},
        {"%r9", {"%r9"}},
    };
    auto it = fam.find(reg);
    return it == fam.end() ? nullptr : &it->second;
}

bool mentions(const std::string& l, const std::vector<std::string>& names) {
    for (const auto& n : names)
        if (contains(l, n)) return true;
    return false;
}

// setCC -> (jump-if-CC, jump-if-not-CC)
const std::unordered_map<std::string, std::pair<std::string, std::string>>& cc_table() {
    static const std::unordered_map<std::string, std::pair<std::string, std::string>> t = {
        {"sete", {"e", "ne"}},  {"setne", {"ne", "e"}}, {"setl", {"l", "ge"}},
        {"setge", {"ge", "l"}}, {"setg", {"g", "le"}},  {"setle", {"le", "g"}},
        {"setb", {"b", "ae"}},  {"setae", {"ae", "b"}}, {"seta", {"a", "be"}},
        {"setbe", {"be", "a"}},
    };
    return t;
}

bool fits32(long long n) { return n >= -2147483648LL && n <= 2147483647LL; }

// "  mov $N, %rax"
bool parse_mov_imm_rax(const std::string& l, long long& n) {
    static const std::string pre = "  mov $";
    static const std::string suf = ", %rax";
    if (!starts_with(l, pre) || !ends_with(l, suf) || l.size() <= pre.size() + suf.size()) return false;
    const std::string num = l.substr(pre.size(), l.size() - pre.size() - suf.size());
    const size_t k = (num[0] == '-') ? 1 : 0;
    if (k >= num.size() || num.size() > 18) return false;
    for (size_t j = k; j < num.size(); ++j)
        if (!std::isdigit(static_cast<unsigned char>(num[j]))) return false;
    n = std::stoll(num);
    return true;
}

bool run_pass(std::vector<std::string>& lines, PeepStats& st) {
    std::vector<std::string> out;
    out.reserve(lines.size());
    bool changed = false;
    bool dead = false;
    const size_t n = lines.size();

    for (size_t i = 0; i < n; ++i) {
        const std::string& l = lines[i];

        // Unreachable instructions after an unconditional jump or return.
        if (dead) {
            if (is_instr(l)) { ++st.dead; changed = true; continue; }
            dead = false;
        }

        // push %rax ; <straight-line code not touching REG or %rsp> ; pop REG   ->   mov %rax, REG ; <code>
        if (l == "  push %rax") {
            size_t j = i + 1;
            while (j < n && j - i <= 8) {
                const std::string& m = lines[j];
                if (starts_with(m, "  pop ")) break;
                if (!is_instr(m) || !is_safe_mnemonic(mnemonic(m)) || contains(m, "%rsp")) { j = n; break; }
                ++j;
            }
            if (j < n && starts_with(lines[j], "  pop ")) {
                const std::string reg = lines[j].substr(6);
                const std::vector<std::string>* fam = family(reg);
                if (fam != nullptr) {
                    bool ok = true;
                    for (size_t k = i + 1; k < j; ++k)
                        if (mentions(lines[k], *fam)) { ok = false; break; }
                    if (ok) {
                        out.push_back("  mov %rax, " + reg);
                        for (size_t k = i + 1; k < j; ++k) out.push_back(lines[k]);
                        i = j;
                        ++st.push_pop;
                        changed = true;
                        continue;
                    }
                }
            }
        }

        // setCC %al ; movzbq %al, %rax ; cmp $0, %rax ; je/jne L   ->   jCC' L
        // (the boolean value in %rax is dead at every such site)
        if (i + 3 < n && starts_with(l, "  set") && ends_with(l, " %al")) {
            const std::string m = mnemonic(l);
            auto it = cc_table().find(m);
            if (it != cc_table().end() && l == "  " + m + " %al" &&
                lines[i + 1] == "  movzbq %al, %rax" && lines[i + 2] == "  cmp $0, %rax") {
                const std::string& j = lines[i + 3];
                if (starts_with(j, "  je ")) {
                    out.push_back("  j" + it->second.second + " " + j.substr(5));
                    i += 3; ++st.branches; changed = true;
                    continue;
                }
                if (starts_with(j, "  jne ")) {
                    out.push_back("  j" + it->second.first + " " + j.substr(6));
                    i += 3; ++st.branches; changed = true;
                    continue;
                }
            }
        }

        // mov %rax,%rcx ; mov $N,%rax ; OP %rcx,%rax   ->   OP $N,%rax   (%rax still holds the lhs)
        if (l == "  mov %rax, %rcx" && i + 2 < n) {
            long long k = 0;
            if (parse_mov_imm_rax(lines[i + 1], k) && fits32(k)) {
                const std::string& op = lines[i + 2];
                std::string repl;
                size_t consumed = 0;
                const std::string ks = std::to_string(k);
                if (op == "  add %rcx, %rax") { repl = "  add $" + ks + ", %rax"; consumed = 2; }
                else if (op == "  and %rcx, %rax") { repl = "  and $" + ks + ", %rax"; consumed = 2; }
                else if (op == "  or %rcx, %rax") { repl = "  or $" + ks + ", %rax"; consumed = 2; }
                else if (op == "  xor %rcx, %rax") { repl = "  xor $" + ks + ", %rax"; consumed = 2; }
                else if (op == "  cmp %rax, %rcx") { repl = "  cmp $" + ks + ", %rax"; consumed = 2; }
                else if (op == "  imul %rcx, %rax") {
                    if (k > 1 && (k & (k - 1)) == 0) {
                        int s = 0;
                        while ((1LL << s) < k) ++s;
                        repl = "  shl $" + std::to_string(s) + ", %rax";
                    } else {
                        repl = "  imul $" + ks + ", %rax, %rax";
                    }
                    consumed = 2;
                } else if (op == "  sub %rax, %rcx" && i + 3 < n && lines[i + 3] == "  mov %rcx, %rax") {
                    repl = "  sub $" + ks + ", %rax";
                    consumed = 3;
                }
                if (consumed != 0) {
                    out.push_back(repl);
                    i += consumed;
                    ++st.immediates;
                    changed = true;
                    continue;
                }
            }
        }

        // jmp to the immediately following label
        if (starts_with(l, "  jmp ") && i + 1 < n && lines[i + 1] == l.substr(6) + ":") {
            changed = true;
            continue;
        }

        // mov %rax, X ; mov X, %rax   ->   mov %rax, X
        if (starts_with(l, "  mov %rax, ") && i + 1 < n) {
            const std::string loc = l.substr(12);
            if (lines[i + 1] == "  mov " + loc + ", %rax") {
                out.push_back(l);
                ++i;
                changed = true;
                continue;
            }
        }

        out.push_back(l);
        if (starts_with(l, "  jmp ") || l == "  ret") dead = true;
    }

    lines.swap(out);
    return changed;
}

}  // namespace

std::string peephole(const std::string& text, PeepStats& stats) {
    std::vector<std::string> lines;
    {
        std::istringstream in(text);
        std::string ln;
        while (std::getline(in, ln)) lines.push_back(ln);
    }
    const size_t before = lines.size();
    for (int round = 0; round < 32; ++round) {
        if (!run_pass(lines, stats)) break;
    }
    stats.removed_lines = static_cast<int>(before) - static_cast<int>(lines.size());

    std::string result;
    result.reserve(text.size());
    for (const auto& l : lines) {
        result += l;
        result += '\n';
    }
    return result;
}