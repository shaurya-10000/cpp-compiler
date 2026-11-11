#pragma once
#include <stdexcept>
#include <string>
#include <vector>

enum class Base { Void, Bool, Char, Int, Double, Struct };

struct Type {
    Base base = Base::Void;
    int ptr = 0;    // pointer depth
    int sid = -1;   // struct id when base == Struct

    bool is_ptr() const { return ptr > 0; }
    bool is_void() const { return ptr == 0 && base == Base::Void; }
    bool is_double() const { return ptr == 0 && base == Base::Double; }
    bool is_struct() const { return ptr == 0 && base == Base::Struct; }
    bool is_integral() const {
        return ptr == 0 && (base == Base::Bool || base == Base::Char || base == Base::Int);
    }
    bool is_scalar() const { return is_integral() || is_double(); }
    Type pointee() const { return Type{base, ptr - 1, sid}; }
    Type pointer_to() const { return Type{base, ptr + 1, sid}; }
    bool operator==(const Type& o) const { return base == o.base && ptr == o.ptr && sid == o.sid; }
    bool operator!=(const Type& o) const { return !(*this == o); }
};

namespace ty {
inline const Type Void{Base::Void, 0, -1};
inline const Type Bool{Base::Bool, 0, -1};
inline const Type Char{Base::Char, 0, -1};
inline const Type Int{Base::Int, 0, -1};
inline const Type Double{Base::Double, 0, -1};
}

struct StructField {
    std::string name;
    Type type;
    int offset = 0;
};

struct StructDef {
    std::string name;
    std::vector<StructField> fields;
    int size = 0;
    int align = 1;
    bool complete = false;
};

inline std::vector<StructDef>& struct_defs() {
    static std::vector<StructDef> defs;
    return defs;
}

inline int find_struct(const std::string& name) {
    auto& d = struct_defs();
    for (size_t i = 0; i < d.size(); ++i)
        if (d[i].name == name) return static_cast<int>(i);
    return -1;
}

inline const StructField* find_field(int sid, const std::string& name) {
    for (const auto& f : struct_defs()[static_cast<size_t>(sid)].fields)
        if (f.name == name) return &f;
    return nullptr;
}

// Accepts names like "int", "char*", "double**", "struct Node*".
inline Type type_from_name(const std::string& s) {
    size_t n = s.size();
    int ptr = 0;
    while (n > 0 && s[n - 1] == '*') { ptr++; n--; }
    const std::string b = s.substr(0, n);
    if (b.rfind("struct ", 0) == 0) {
        const std::string nm = b.substr(7);
        int id = find_struct(nm);
        if (id < 0) throw std::runtime_error("unknown struct '" + nm + "'");
        return Type{Base::Struct, ptr, id};
    }
    Base base = Base::Int;
    if (b == "int") base = Base::Int;
    else if (b == "bool") base = Base::Bool;
    else if (b == "char") base = Base::Char;
    else if (b == "double") base = Base::Double;
    else if (b == "void") base = Base::Void;
    else throw std::runtime_error("unknown type '" + s + "'");
    return Type{base, ptr, -1};
}

inline std::string type_name(const Type& t) {
    std::string b = "void";
    switch (t.base) {
        case Base::Void: b = "void"; break;
        case Base::Bool: b = "bool"; break;
        case Base::Char: b = "char"; break;
        case Base::Int: b = "int"; break;
        case Base::Double: b = "double"; break;
        case Base::Struct: b = "struct " + struct_defs()[static_cast<size_t>(t.sid)].name; break;
    }
    return b + std::string(static_cast<size_t>(t.ptr), '*');
}

inline int align_of(const Type& t) {
    if (t.ptr > 0) return 8;
    switch (t.base) {
        case Base::Bool: case Base::Char: return 1;
        case Base::Int: case Base::Double: return 8;
        case Base::Struct: return struct_defs()[static_cast<size_t>(t.sid)].align;
        case Base::Void: break;
    }
    throw std::runtime_error("alignment of void");
}

// int is 64-bit in minicc.
inline int size_of(const Type& t) {
    if (t.ptr > 0) return 8;
    switch (t.base) {
        case Base::Bool: case Base::Char: return 1;
        case Base::Int: case Base::Double: return 8;
        case Base::Struct: {
            const StructDef& d = struct_defs()[static_cast<size_t>(t.sid)];
            if (!d.complete) throw std::runtime_error("size of incomplete struct '" + d.name + "'");
            return d.size;
        }
        case Base::Void: break;
    }
    throw std::runtime_error("size of void");
}