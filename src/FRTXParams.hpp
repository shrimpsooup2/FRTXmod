#pragma once

// Runtime description of every tunable setting, built from the same X-macro
// list the config struct and mod.json come from. The in-game tuner walks this
// table, so a new setting shows up there the moment it is added to the spec.

enum class FRTXParamType {
    Section,
    Float,
    Int,
    Bool,
};

struct FRTXParam {
    char const* key;   // null for section headers
    char const* label;
    FRTXParamType type;
    double min;
    double max;
    double def;
    double step;
};

extern FRTXParam const kFRTXParams[];
extern int const kFRTXParamCount;
