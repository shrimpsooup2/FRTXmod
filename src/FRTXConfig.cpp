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

    bool readBool(char const* key, bool) {
        return Mod::get()->getSettingValue<bool>(key);
    }

    FRTXColor readColor(char const* key, int, int, int) {
        auto c = Mod::get()->getSettingValue<cocos2d::ccColor3B>(key);
        return FRTXColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f};
    }

    // The presets are the point of the mod for most people, so each carries the
    // whole look. A set of values that agree with each other beats forty-odd
    // controls that are each individually reasonable.
    void presetInto(FRTXConfig& cfg, FRTXPreset preset) {
        if (preset == FRTXPreset::Custom) return;

        // Everything the presets share. Colours and the light ray shape are
        // left alone so a preset never silently discards a tint the user
        // picked, and rays stay off unless a preset explicitly wants them.
        cfg.bloomEnabled = true;
        cfg.bloomLevels = 3;
        cfg.bloomScale = 0.5f;
        cfg.tonemapEnabled = true;
        cfg.temperature = 0.0f;
        cfg.tint = 0.0f;
        cfg.streakIntensity = 0.0f;
        cfg.raysIntensity = 0.0f;
        cfg.dither = true;
        cfg.vignetteRoundness = 0.5f;
        cfg.vignetteSoftness = 0.5f;

        switch (preset) {
            case FRTXPreset::Custom:
                return;

            case FRTXPreset::Subtle:
                cfg.bloomIntensity = 0.85f;
                cfg.bloomThreshold = 0.70f;
                cfg.bloomKnee = 0.35f;
                cfg.bloomRadius = 1.4f;
                cfg.bloomLevels = 2;
                cfg.bloomSpread = 0.65f;
                cfg.emissiveBias = 0.50f;
                cfg.exposure = 1.0f;
                cfg.contrast = 1.04f;
                cfg.saturation = 1.10f;
                cfg.clarity = 0.08f;
                cfg.clarityRadius = 3.0f;
                cfg.blackPoint = 0.020f;
                cfg.splitShadow = 0.10f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.05f;
                cfg.chromatic = 0.0f;
                cfg.grain = 0.0f;
                return;

            // Tuned against a frame of an RTX showcase level: large soft radial
            // halos, no anamorphic streaking, crushed blacks, and a grade that
            // stays out of the way so the level's own neon palette carries the
            // image.
            case FRTXPreset::Showcase:
                cfg.bloomIntensity = 1.60f;
                cfg.bloomThreshold = 0.52f;
                cfg.bloomKnee = 0.45f;
                cfg.bloomRadius = 2.0f;
                cfg.bloomSpread = 0.80f;
                cfg.emissiveBias = 0.55f;
                cfg.exposure = 1.05f;
                cfg.contrast = 1.10f;
                cfg.saturation = 1.25f;
                cfg.clarity = 0.12f;
                cfg.clarityRadius = 3.5f;
                cfg.blackPoint = 0.050f;
                cfg.splitShadow = 0.20f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.10f;
                cfg.chromatic = 0.06f;
                cfg.grain = 0.0f;
                return;

            // Every knob that costs fill rate turned down or off: no streaks,
            // no rays, no clarity (which is 4 to 8 full resolution fetches per
            // pixel), no chromatic aberration (2 more), smaller bloom buffers
            // and one fewer blur level.
            case FRTXPreset::Performance:
                cfg.bloomIntensity = 1.30f;
                cfg.bloomThreshold = 0.60f;
                cfg.bloomKnee = 0.40f;
                cfg.bloomRadius = 1.6f;
                cfg.bloomLevels = 2;
                cfg.bloomScale = 0.35f;
                cfg.bloomSpread = 0.70f;
                cfg.emissiveBias = 0.55f;
                cfg.exposure = 1.03f;
                cfg.contrast = 1.08f;
                cfg.saturation = 1.18f;
                cfg.clarity = 0.0f;
                cfg.clarityTaps = 4;
                cfg.blackPoint = 0.040f;
                cfg.splitShadow = 0.15f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.0f;
                cfg.chromatic = 0.0f;
                cfg.grain = 0.0f;
                return;

            case FRTXPreset::Overkill:
                cfg.bloomIntensity = 2.60f;
                cfg.bloomThreshold = 0.35f;
                cfg.bloomKnee = 0.50f;
                cfg.bloomRadius = 2.8f;
                cfg.bloomSpread = 0.95f;
                cfg.emissiveBias = 0.50f;
                cfg.streakIntensity = 0.50f;
                cfg.streakLength = 2.0f;
                cfg.raysIntensity = 0.45f;
                cfg.exposure = 1.12f;
                cfg.contrast = 1.18f;
                cfg.saturation = 1.45f;
                cfg.clarity = 0.25f;
                cfg.clarityRadius = 5.0f;
                cfg.blackPoint = 0.075f;
                cfg.splitShadow = 0.35f;
                cfg.splitHighlight = 0.15f;
                cfg.vignette = 0.30f;
                cfg.chromatic = 0.25f;
                cfg.grain = 0.020f;
                return;
        }
    }
}

FRTXConfig FRTXConfig::read() {
    FRTXConfig cfg;

#define FRTX_FLOAT(key, member, label, lo, hi, def, step) cfg.member = readFloat(key, def, lo, hi);
#define FRTX_INT(key, member, label, lo, hi, def, step)   cfg.member = readInt(key, def, lo, hi);
#define FRTX_BOOL(key, member, label, def)                cfg.member = readBool(key, def);
#define FRTX_COLOR(key, member, label, r, g, b)           cfg.member = readColor(key, r, g, b);
#include "FRTXParams.inc"

    // Passes that are on but contribute nothing are just wasted bandwidth.
    if (cfg.bloomIntensity <= 0.0f && cfg.streakIntensity <= 0.0f && cfg.raysIntensity <= 0.0f) {
        cfg.bloomEnabled = false;
    }

    return cfg;
}

namespace {
    FRTXConfig g_cached;
    bool g_cacheValid = false;
    unsigned g_generation = 1;
}

void FRTXConfig::applyPreset(FRTXPreset preset) {
    // Start from what the user currently has, so settings the preset does not
    // mention -- the master switch, the debug view, colour tints -- survive.
    FRTXConfig cfg = current();
    presetInto(cfg, preset);

    auto mod = Mod::get();
#define FRTX_FLOAT(key, member, label, lo, hi, def, step) \
    mod->setSettingValue<double>(key, static_cast<double>(cfg.member));
#define FRTX_INT(key, member, label, lo, hi, def, step) \
    mod->setSettingValue<int64_t>(key, static_cast<int64_t>(cfg.member));
#define FRTX_BOOL(key, member, label, def) \
    mod->setSettingValue<bool>(key, cfg.member);
// Colours are the user's own choice; no preset speaks for them.
#define FRTX_COLOR(key, member, label, r, g, b)
#include "FRTXParams.inc"

    invalidate();
}

FRTXConfig const& FRTXConfig::current() {
    if (!g_cacheValid) {
        g_cached = read();
        g_cacheValid = true;
    }
    return g_cached;
}

void FRTXConfig::invalidate() {
    g_cacheValid = false;
    ++g_generation;
}

unsigned FRTXConfig::generation() {
    return g_generation;
}

bool FRTXConfig::isNoOp() const {
    if (!enabled) return true;
    // The debug views are only meaningful while we are actually capturing.
    if (debugView != 0) return false;
    if (bloomEnabled) return false;
    if (tonemapEnabled) return false;
    if (vignette > 0.0f || chromatic > 0.0f || grain > 0.0f) return false;
    if (clarity > 0.0f || blackPoint > 0.0f) return false;
    if (splitShadow != 0.0f || splitHighlight != 0.0f) return false;
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
        || bloomScale != other.bloomScale
        || streaksEnabled() != other.streaksEnabled()
        || raysEnabled() != other.raysEnabled();
}
