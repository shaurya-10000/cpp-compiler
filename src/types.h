#pragma once
#include <stdexcept>
#include <string>

enum class Type { Void, Bool, Char, Int, Double, Str };

inline Type type_from_name(const std::string& n) {
    if (n == "int") return Type::Int;
    if (n == "bool") return Type::Bool;
    if (n == "char") return Type::Char;
    if (n == "double") return Type::Double;
    if (n == "void") return Type::Void;
    throw std::runtime_error("unknown type '" + n + "'");
}

inline const char* type_name(Type t) {
    switch (t) {
        case Type::Void: return "void";
        case Type::Bool: return "bool";
        case Type::Char: return "char";
        case Type::Int: return "int";
        case Type::Double: return "double";
        case Type::Str: return "string";
    }
    return "?";
}