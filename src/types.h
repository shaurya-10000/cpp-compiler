#pragma once
#include <stdexcept>
#include <string>

enum class Base { Void, Bool, Char, Int, Double };

struct Type {
    Base base = Base::Void;
    int ptr = 0;   // pointer depth: 0 = plain value, 1 = T*, 2 = T**, ...

    bool is_ptr() const { return ptr > 0; }
    bool is_void() const { return ptr == 0 && base == Base::Void; }
    bool is_double() const { return ptr == 0 && base == Base::Double; }
    bool is_integral() const {
        return ptr == 0 && (base == Base::Bool || base == Base::Char || base == Base::Int);
    }
    bool is_scalar() const { return is_integral() || is_double(); }
    Type pointee() const { return Type{base, ptr - 1}; }
    Type pointer_to() const { return Type{base, ptr + 1}; }
    bool operator==(const Type& o) const { return base == o.base && ptr == o.ptr; }
    bool operator!=(const Type& o) const { return !(*this == o); }
};

namespace ty {
inline const Type Void{Base::Void, 0};
inline const Type Bool{Base::Bool, 0};
inline const Type Char{Base::Char, 0};
inline const Type Int{Base::Int, 0};
inline const Type Double{Base::Double, 0};
}

// Accepts names like "int", "char*", "double**".
inline Type type_from_name(const std::string& s) {
    size_t n = s.size();
    int ptr = 0;
    while (n > 0 && s[n - 1] == '*') { ptr++; n--; }
    const std::string b = s.substr(0, n);
    Base base = Base::Int;
    if (b == "int") base = Base::Int;
    else if (b == "bool") base = Base::Bool;
    else if (b == "char") base = Base::Char;
    else if (b == "double") base = Base::Double;
    else if (b == "void") base = Base::Void;
    else throw std::runtime_error("unknown type '" + s + "'");
    return Type{base, ptr};
}

inline std::string type_name(const Type& t) {
    const char* b = "void";
    switch (t.base) {
        case Base::Void: b = "void"; break;
        case Base::Bool: b = "bool"; break;
        case Base::Char: b = "char"; break;
        case Base::Int: b = "int"; break;
        case Base::Double: b = "double"; break;
    }
    return std::string(b) + std::string(static_cast<size_t>(t.ptr), '*');
}

// int is 64-bit in minicc.
inline int size_of(const Type& t) {
    if (t.ptr > 0) return 8;
    switch (t.base) {
        case Base::Bool: case Base::Char: return 1;
        case Base::Int: case Base::Double: return 8;
        case Base::Void: break;
    }
    throw std::runtime_error("size of void");
}