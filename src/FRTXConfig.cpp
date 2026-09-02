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

    // The presets are the point of the mod for most people, so they carry the
    // whole look: a set of sliders that agree with each other beats twenty-odd
    // controls that each individually do something reasonable.
    void applyPreset(FRTXConfig& cfg, FRTXPreset preset) {
        switch (preset) {
            case FRTXPreset::Custom:
                return;

            case FRTXPreset::Subtle:
                cfg.bloomEnabled = true;
                cfg.bloomIntensity = 0.85f;
                cfg.bloomThreshold = 0.70f;
                cfg.bloomKnee = 0.35f;
                cfg.bloomRadius = 1.4f;
                cfg.bloomLevels = 2;
                cfg.bloomScale = 0.5f;
                cfg.bloomSpread = 0.65f;
                cfg.emissiveBias = 0.50f;
                cfg.streakIntensity = 0.0f;
                cfg.streakLength = 1.4f;
                cfg.tonemapEnabled = true;
                cfg.exposure = 1.0f;
                cfg.contrast = 1.04f;
                cfg.saturation = 1.10f;
                cfg.temperature = 0.0f;
                cfg.tint = 0.0f;
                cfg.clarity = 0.08f;
                cfg.clarityRadius = 3.0f;
                cfg.blackPoint = 0.020f;
                cfg.splitShadow = 0.10f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.05f;
                cfg.chromatic = 0.0f;
                cfg.grain = 0.0f;
                cfg.dither = true;
                return;

            // Tuned against a frame of an RTX showcase level: large soft radial
            // halos, no anamorphic streaking, crushed blacks, and a grade that
            // stays out of the way so the level's own neon palette carries the
            // image.
            case FRTXPreset::Showcase:
                cfg.bloomEnabled = true;
                cfg.bloomIntensity = 1.60f;
                cfg.bloomThreshold = 0.52f;
                cfg.bloomKnee = 0.45f;
                cfg.bloomRadius = 2.0f;
                cfg.bloomLevels = 3;
                cfg.bloomScale = 0.5f;
                cfg.bloomSpread = 0.80f;
                cfg.emissiveBias = 0.55f;
                cfg.streakIntensity = 0.0f;
                cfg.streakLength = 1.4f;
                cfg.tonemapEnabled = true;
                cfg.exposure = 1.05f;
                cfg.contrast = 1.10f;
                cfg.saturation = 1.25f;
                cfg.temperature = 0.0f;
                cfg.tint = 0.0f;
                cfg.clarity = 0.12f;
                cfg.clarityRadius = 3.5f;
                cfg.blackPoint = 0.050f;
                cfg.splitShadow = 0.20f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.10f;
                cfg.chromatic = 0.06f;
                cfg.grain = 0.0f;
                cfg.dither = true;
                return;

            case FRTXPreset::Overkill:
                cfg.bloomEnabled = true;
                cfg.bloomIntensity = 2.60f;
                cfg.bloomThreshold = 0.35f;
                cfg.bloomKnee = 0.50f;
                cfg.bloomRadius = 2.8f;
                cfg.bloomLevels = 3;
                cfg.bloomScale = 0.5f;
                cfg.bloomSpread = 0.95f;
                cfg.emissiveBias = 0.50f;
                cfg.streakIntensity = 0.50f;
                cfg.streakLength = 2.0f;
                cfg.tonemapEnabled = true;
                cfg.exposure = 1.12f;
                cfg.contrast = 1.18f;
                cfg.saturation = 1.45f;
                cfg.temperature = 0.0f;
                cfg.tint = 0.0f;
                cfg.clarity = 0.25f;
                cfg.clarityRadius = 5.0f;
                cfg.blackPoint = 0.075f;
                cfg.splitShadow = 0.35f;
                cfg.splitHighlight = 0.15f;
                cfg.vignette = 0.30f;
                cfg.chromatic = 0.25f;
                cfg.grain = 0.020f;
                cfg.dither = true;
                return;
        }
    }
}

FRTXConfig FRTXConfig::read() {
    auto mod = Mod::get();

    FRTXConfig cfg;
    cfg.enabled        = mod->getSettingValue<bool>("enabled");

    cfg.bloomEnabled   = mod->getSettingValue<bool>("bloom-enabled");
    cfg.bloomIntensity = readFloat("bloom-intensity", cfg.bloomIntensity, 0.0f, 4.0f);
    cfg.bloomThreshold = readFloat("bloom-threshold", cfg.bloomThreshold, 0.0f, 1.5f);
    cfg.bloomKnee      = readFloat("bloom-knee", cfg.bloomKnee, 0.0f, 1.0f);
    cfg.bloomRadius    = readFloat("bloom-radius", cfg.bloomRadius, 0.25f, 4.0f);
    cfg.bloomLevels    = readInt("bloom-levels", cfg.bloomLevels, 1, 3);
    cfg.bloomScale     = readFloat("bloom-scale", cfg.bloomScale, 0.25f, 1.0f);
    cfg.bloomSpread    = readFloat("bloom-spread", cfg.bloomSpread, 0.0f, 1.0f);
    cfg.emissiveBias   = readFloat("bloom-emissive-bias", cfg.emissiveBias, 0.0f, 1.0f);

    cfg.streakIntensity = readFloat("streak-intensity", cfg.streakIntensity, 0.0f, 2.0f);
    cfg.streakLength    = readFloat("streak-length", cfg.streakLength, 0.25f, 4.0f);

    cfg.tonemapEnabled = mod->getSettingValue<bool>("tonemap-enabled");
    cfg.exposure       = readFloat("exposure", cfg.exposure, 0.25f, 2.5f);
    cfg.contrast       = readFloat("contrast", cfg.contrast, 0.5f, 2.0f);
    cfg.saturation     = readFloat("saturation", cfg.saturation, 0.0f, 2.0f);
    cfg.temperature    = readFloat("temperature", cfg.temperature, -1.0f, 1.0f);
    cfg.tint           = readFloat("tint", cfg.tint, -1.0f, 1.0f);

    cfg.clarity        = readFloat("clarity", cfg.clarity, 0.0f, 1.5f);
    cfg.clarityRadius  = readFloat("clarity-radius", cfg.clarityRadius, 1.0f, 16.0f);

    cfg.blackPoint     = readFloat("black-point", cfg.blackPoint, 0.0f, 0.2f);
    cfg.splitShadow    = readFloat("split-shadow", cfg.splitShadow, -1.0f, 1.0f);
    cfg.splitHighlight = readFloat("split-highlight", cfg.splitHighlight, -1.0f, 1.0f);

    cfg.vignette       = readFloat("vignette", cfg.vignette, 0.0f, 1.0f);
    cfg.chromatic      = readFloat("chromatic", cfg.chromatic, 0.0f, 1.0f);
    cfg.grain          = readFloat("grain", cfg.grain, 0.0f, 0.15f);
    cfg.dither         = mod->getSettingValue<bool>("dither");

    cfg.debugView      = readInt("debug-view", 0, 0, 3);

    // A preset overrides every look control. `enabled` and the debug view stay
    // under the user's hand so a preset can never lock them out.
    applyPreset(cfg, static_cast<FRTXPreset>(readInt("preset", 2, 0, 3)));

    // Bloom that is on but contributes nothing is just wasted passes.
    if (cfg.bloomIntensity <= 0.0f && cfg.streakIntensity <= 0.0f) {
        cfg.bloomEnabled = false;
    }

    return cfg;
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
        || streaksEnabled() != other.streaksEnabled();
}
