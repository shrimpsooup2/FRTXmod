#include "FRTXConfig.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    float readFloat(char const* key, float fallback, float lo, float hi) {
        auto value = static_cast<float>(Mod::get()->getSettingValue<double>(key));
        if (!std::isfinite(value)) return fallback;
        return std::clamp(value, lo, hi);
    }

    int readInt(char const* key, int fallback, int lo, int hi) {
        auto value = static_cast<int>(Mod::get()->getSettingValue<int64_t>(key));
        if (value < lo || value > hi) return fallback;
        return value;
    }
}

FRTXConfig FRTXConfig::read() {
    auto mod = Mod::get();

    FRTXConfig cfg;
    cfg.enabled        = mod->getSettingValue<bool>("enabled");

    cfg.bloomEnabled   = mod->getSettingValue<bool>("bloom-enabled");
    cfg.bloomIntensity = readFloat("bloom-intensity", 1.0f, 0.0f, 4.0f);
    cfg.bloomThreshold = readFloat("bloom-threshold", 0.75f, 0.0f, 1.5f);
    cfg.bloomKnee      = readFloat("bloom-knee", 0.4f, 0.0f, 1.0f);
    cfg.bloomRadius    = readFloat("bloom-radius", 1.0f, 0.25f, 4.0f);
    cfg.bloomLevels    = readInt("bloom-levels", 3, 1, 3);
    cfg.bloomScale     = readFloat("bloom-scale", 0.5f, 0.25f, 1.0f);

    cfg.tonemapEnabled = mod->getSettingValue<bool>("tonemap-enabled");
    cfg.exposure       = readFloat("exposure", 1.0f, 0.25f, 2.5f);
    cfg.contrast       = readFloat("contrast", 1.05f, 0.5f, 2.0f);
    cfg.saturation     = readFloat("saturation", 1.08f, 0.0f, 2.0f);
    cfg.temperature    = readFloat("temperature", 0.15f, -1.0f, 1.0f);
    cfg.tint           = readFloat("tint", 0.0f, -1.0f, 1.0f);

    cfg.vignette       = readFloat("vignette", 0.25f, 0.0f, 1.0f);
    cfg.chromatic      = readFloat("chromatic", 0.12f, 0.0f, 1.0f);
    cfg.grain          = readFloat("grain", 0.0f, 0.0f, 0.15f);
    cfg.dither         = mod->getSettingValue<bool>("dither");

    cfg.debugView      = readInt("debug-view", 0, 0, 2);

    // Bloom that is on but contributes nothing is just wasted passes.
    if (cfg.bloomIntensity <= 0.0f) cfg.bloomEnabled = false;

    return cfg;
}

bool FRTXConfig::isNoOp() const {
    if (!enabled) return true;
    // The debug views are only meaningful while we are actually capturing.
    if (debugView != 0) return false;
    if (bloomEnabled) return false;
    if (tonemapEnabled) return false;
    if (vignette > 0.0f || chromatic > 0.0f || grain > 0.0f) return false;
    if (dither) return false;
    // Anything that leaves the image mathematically unchanged.
    return exposure == 1.0f
        && contrast == 1.0f
        && saturation == 1.0f
        && temperature == 0.0f
        && tint == 0.0f;
}

bool FRTXConfig::needsRealloc(FRTXConfig const& other) const {
    return bloomEnabled != other.bloomEnabled
        || bloomLevels != other.bloomLevels
        || bloomScale != other.bloomScale;
}
