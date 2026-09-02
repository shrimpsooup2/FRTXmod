#include "FRTXParams.hpp"

FRTXParam const kFRTXParams[] = {
#define FRTX_SECTION(label) \
    {nullptr, label, FRTXParamType::Section, 0.0, 0.0, 0.0, 0.0},
#define FRTX_FLOAT(key, member, label, lo, hi, def, step) \
    {key, label, FRTXParamType::Float, lo, hi, def, step},
#define FRTX_INT(key, member, label, lo, hi, def, step) \
    {key, label, FRTXParamType::Int, \
     static_cast<double>(lo), static_cast<double>(hi), \
     static_cast<double>(def), static_cast<double>(step)},
#define FRTX_BOOL(key, member, label, def) \
    {key, label, FRTXParamType::Bool, 0.0, 1.0, (def) ? 1.0 : 0.0, 1.0},
// Colours are picked in the Geode settings menu; a keyboard overlay is the
// wrong instrument for them.
#define FRTX_COLOR(key, member, label, r, g, b)
#include "FRTXParams.inc"
};

int const kFRTXParamCount = static_cast<int>(sizeof(kFRTXParams) / sizeof(kFRTXParams[0]));
