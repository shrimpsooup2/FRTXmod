#include "FRTXConfig.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace geode::prelude;

FRTXPresetInfo const kFRTXPresets[] = {
#define FRTX_PRESET(name, id, label) {id, label, FRTXPreset::name},
#include "FRTXPresets.inc"
};

int const kFRTXPresetCount = static_cast<int>(sizeof(kFRTXPresets) / sizeof(kFRTXPresets[0]));

bool frtxPresetFromId(char const* id, FRTXPreset& out) {
    for (int i = 0; i < kFRTXPresetCount; ++i) {
        if (std::strcmp(kFRTXPresets[i].id, id) == 0) {
            out = kFRTXPresets[i].preset;
            return true;
        }
    }
    return false;
}

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
        cfg.bgRadius = 0.14f;
        // Ray shape and lens extras start from neutral, so a preset that does
        // not mention them cannot inherit another preset's sun.
        cfg.raysJitter = 0.9f;
        cfg.raysOcclusion = 0.5f;
        cfg.raysShimmer = 0.0f;
        cfg.raysSun = 0.0f;
        cfg.raysSunSize = 0.08f;
        cfg.raysOriginMode = 0;
        cfg.flareIntensity = 0.0f;
        cfg.flareSpacing = 0.35f;
        cfg.halation = 0.0f;
        cfg.lensDistortion = 0.0f;

        switch (preset) {
            // Built for glow-heavy levels: only strongly saturated things are
            // allowed to bloom, and broad bright areas are held back hard.
            case FRTXPreset::Neon:
                cfg.bgSuppress = 0.55f;
                cfg.bloomIntensity = 1.50f;
                cfg.bloomThreshold = 0.55f;
                cfg.bloomKnee = 0.35f;
                cfg.bloomRadius = 1.8f;
                cfg.bloomSpread = 0.70f;
                cfg.emissiveBias = 0.85f;
                cfg.exposure = 1.05f;
                cfg.contrast = 1.12f;
                cfg.saturation = 1.40f;
                cfg.clarity = 0.10f;
                cfg.clarityRadius = 3.0f;
                cfg.blackPoint = 0.060f;
                cfg.splitShadow = 0.35f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.15f;
                cfg.chromatic = 0.08f;
                cfg.grain = 0.0f;
                return;

            case FRTXPreset::Cinematic:
                cfg.bgSuppress = 0.40f;
                cfg.bloomIntensity = 1.10f;
                cfg.bloomThreshold = 0.70f;
                cfg.bloomKnee = 0.40f;
                cfg.bloomRadius = 2.2f;
                cfg.bloomSpread = 0.85f;
                cfg.emissiveBias = 0.40f;
                cfg.exposure = 1.02f;
                cfg.contrast = 1.15f;
                cfg.saturation = 1.05f;
                cfg.temperature = 0.08f;
                cfg.clarity = 0.25f;
                cfg.clarityRadius = 6.0f;
                cfg.blackPoint = 0.055f;
                cfg.splitShadow = 0.45f;
                cfg.splitHighlight = 0.40f;
                cfg.vignette = 0.45f;
                cfg.chromatic = 0.20f;
                cfg.grain = 0.020f;
                return;

            // Wide, low threshold and low contrast: haze rather than glow.
            case FRTXPreset::Dreamy:
                cfg.bgSuppress = 0.10f;
                cfg.bloomIntensity = 1.50f;
                cfg.bloomThreshold = 0.40f;
                cfg.bloomKnee = 0.60f;
                cfg.bloomRadius = 3.0f;
                cfg.bloomSpread = 0.95f;
                cfg.emissiveBias = 0.30f;
                cfg.exposure = 1.05f;
                cfg.contrast = 0.95f;
                cfg.saturation = 1.10f;
                cfg.clarity = 0.0f;
                cfg.blackPoint = 0.0f;
                cfg.splitShadow = 0.20f;
                cfg.splitHighlight = 0.20f;
                cfg.vignette = 0.25f;
                cfg.chromatic = 0.10f;
                cfg.grain = 0.0f;
                return;

            case FRTXPreset::Noir:
                cfg.bgSuppress = 0.35f;
                cfg.bloomIntensity = 1.00f;
                cfg.bloomThreshold = 0.70f;
                cfg.bloomKnee = 0.30f;
                cfg.bloomRadius = 1.8f;
                cfg.bloomSpread = 0.60f;
                cfg.emissiveBias = 0.20f;
                cfg.exposure = 1.0f;
                cfg.contrast = 1.45f;
                cfg.saturation = 0.0f;
                cfg.clarity = 0.35f;
                cfg.clarityRadius = 5.0f;
                cfg.blackPoint = 0.090f;
                cfg.splitShadow = 0.0f;
                cfg.splitHighlight = 0.0f;
                cfg.vignette = 0.55f;
                cfg.chromatic = 0.05f;
                cfg.grain = 0.040f;
                return;

            // Punch without haze: colour and contrast do the work, not glow.
            case FRTXPreset::Vivid:
                cfg.bgSuppress = 0.50f;
                cfg.bloomIntensity = 0.60f;
                cfg.bloomThreshold = 0.85f;
                cfg.bloomKnee = 0.25f;
                cfg.bloomRadius = 1.2f;
                cfg.bloomLevels = 2;
                cfg.bloomSpread = 0.50f;
                cfg.emissiveBias = 0.70f;
                cfg.exposure = 1.03f;
                cfg.contrast = 1.20f;
                cfg.saturation = 1.50f;
                cfg.clarity = 0.30f;
                cfg.clarityRadius = 4.0f;
                cfg.blackPoint = 0.050f;
                cfg.splitShadow = 0.10f;
                cfg.splitHighlight = 0.10f;
                cfg.vignette = 0.10f;
                cfg.chromatic = 0.0f;
                cfg.grain = 0.0f;
                return;

            // The one preset that turns light rays on.
            case FRTXPreset::Sunbeam:
                cfg.bgSuppress = 0.30f;
                cfg.bloomIntensity = 1.40f;
                cfg.bloomThreshold = 0.60f;
                cfg.bloomKnee = 0.40f;
                cfg.bloomRadius = 2.0f;
                cfg.bloomSpread = 0.85f;
                cfg.emissiveBias = 0.50f;
                cfg.raysIntensity = 0.60f;
                cfg.raysDensity = 0.90f;
                cfg.raysDecay = 0.960f;
                cfg.raysWeight = 0.40f;
                cfg.raysSamples = 28;
                cfg.exposure = 1.06f;
                cfg.contrast = 1.10f;
                cfg.saturation = 1.20f;
                cfg.temperature = 0.20f;
                cfg.clarity = 0.15f;
                cfg.clarityRadius = 4.0f;
                cfg.blackPoint = 0.045f;
                cfg.splitShadow = 0.30f;
                cfg.splitHighlight = 0.40f;
                cfg.vignette = 0.30f;
                cfg.chromatic = 0.12f;
                cfg.grain = 0.0f;
                return;

            // A low warm sun throwing long shafts across the frame.
            case FRTXPreset::Dawn:
                cfg.bgSuppress = 0.30f;
                cfg.bloomIntensity = 1.50f;
                cfg.bloomThreshold = 0.55f;
                cfg.bloomKnee = 0.40f;
                cfg.bloomRadius = 2.2f;
                cfg.bloomSpread = 0.85f;
                cfg.emissiveBias = 0.45f;
                cfg.raysIntensity = 0.75f;
                cfg.raysDensity = 1.10f;
                cfg.raysDecay = 0.970f;
                cfg.raysWeight = 0.42f;
                cfg.raysSamples = 32;
                cfg.raysOriginX = 0.50f;
                cfg.raysOriginY = 0.12f;
                cfg.raysSun = 0.50f;
                cfg.raysSunSize = 0.10f;
                cfg.raysShimmer = 0.15f;
                cfg.raysOcclusion = 0.60f;
                cfg.exposure = 1.06f;
                cfg.contrast = 1.08f;
                cfg.saturation = 1.20f;
                cfg.temperature = 0.35f;
                cfg.clarity = 0.12f;
                cfg.clarityRadius = 4.0f;
                cfg.blackPoint = 0.040f;
                cfg.splitShadow = 0.30f;
                cfg.splitHighlight = 0.50f;
                cfg.vignette = 0.35f;
                cfg.chromatic = 0.12f;
                cfg.halation = 0.25f;
                cfg.grain = 0.010f;
                return;

            // Hard shafts dropping from a source above the frame. The origin
            // sits off screen, which is what makes them parallel rather than
            // fanning out of a visible point.
            case FRTXPreset::Cathedral:
                cfg.bgSuppress = 0.45f;
                cfg.bloomIntensity = 1.20f;
                cfg.bloomThreshold = 0.65f;
                cfg.bloomKnee = 0.35f;
                cfg.bloomRadius = 1.8f;
                cfg.bloomSpread = 0.75f;
                cfg.emissiveBias = 0.50f;
                cfg.raysIntensity = 0.90f;
                cfg.raysDensity = 0.75f;
                cfg.raysDecay = 0.940f;
                cfg.raysWeight = 0.45f;
                cfg.raysSamples = 36;
                cfg.raysOriginX = 0.50f;
                cfg.raysOriginY = 1.15f;
                cfg.raysShimmer = 0.10f;
                cfg.raysOcclusion = 0.75f;
                cfg.exposure = 1.0f;
                cfg.contrast = 1.25f;
                cfg.saturation = 1.05f;
                cfg.temperature = -0.10f;
                cfg.clarity = 0.20f;
                cfg.clarityRadius = 5.0f;
                cfg.blackPoint = 0.070f;
                cfg.splitShadow = 0.50f;
                cfg.splitHighlight = 0.15f;
                cfg.vignette = 0.50f;
                cfg.chromatic = 0.08f;
                cfg.grain = 0.0f;
                return;

            // Dark frame around a small, fierce corona.
            case FRTXPreset::Eclipse:
                cfg.bgSuppress = 0.60f;
                cfg.bloomIntensity = 1.10f;
                cfg.bloomThreshold = 0.72f;
                cfg.bloomKnee = 0.30f;
                cfg.bloomRadius = 1.6f;
                cfg.bloomSpread = 0.60f;
                cfg.emissiveBias = 0.60f;
                cfg.raysIntensity = 1.20f;
                cfg.raysDensity = 0.55f;
                cfg.raysDecay = 0.900f;
                cfg.raysWeight = 0.50f;
                cfg.raysSamples = 40;
                cfg.raysOriginX = 0.50f;
                cfg.raysOriginY = 0.60f;
                cfg.raysSun = 1.20f;
                cfg.raysSunSize = 0.045f;
                cfg.raysShimmer = 0.25f;
                cfg.raysOcclusion = 0.35f;
                cfg.exposure = 0.95f;
                cfg.contrast = 1.30f;
                cfg.saturation = 1.10f;
                cfg.clarity = 0.20f;
                cfg.clarityRadius = 4.0f;
                cfg.blackPoint = 0.100f;
                cfg.splitShadow = 0.40f;
                cfg.splitHighlight = 0.10f;
                cfg.vignette = 0.60f;
                cfg.chromatic = 0.18f;
                cfg.flareIntensity = 0.30f;
                cfg.flareSpacing = 0.30f;
                cfg.grain = 0.0f;
                return;

            // Cool, hazy and slow-moving: the shimmer does most of the work.
            case FRTXPreset::Aurora:
                cfg.bgSuppress = 0.15f;
                cfg.bloomIntensity = 1.70f;
                cfg.bloomThreshold = 0.45f;
                cfg.bloomKnee = 0.55f;
                cfg.bloomRadius = 2.8f;
                cfg.bloomSpread = 0.95f;
                cfg.emissiveBias = 0.40f;
                cfg.raysIntensity = 0.50f;
                cfg.raysDensity = 1.30f;
                cfg.raysDecay = 0.975f;
                cfg.raysWeight = 0.30f;
                cfg.raysSamples = 30;
                cfg.raysOriginX = 0.50f;
                cfg.raysOriginY = 1.05f;
                cfg.raysShimmer = 0.50f;
                cfg.raysOcclusion = 0.30f;
                cfg.exposure = 1.05f;
                cfg.contrast = 1.0f;
                cfg.saturation = 1.30f;
                cfg.temperature = -0.25f;
                cfg.tint = 0.15f;
                cfg.clarity = 0.0f;
                cfg.blackPoint = 0.020f;
                cfg.splitShadow = 0.35f;
                cfg.splitHighlight = -0.20f;
                cfg.vignette = 0.20f;
                cfg.chromatic = 0.10f;
                cfg.grain = 0.0f;
                return;

            // A hot sun close to the camera, with film halation doing the rest.
            case FRTXPreset::Inferno:
                cfg.bgSuppress = 0.20f;
                cfg.bloomIntensity = 2.00f;
                cfg.bloomThreshold = 0.50f;
                cfg.bloomKnee = 0.45f;
                cfg.bloomRadius = 2.4f;
                cfg.bloomSpread = 0.90f;
                cfg.emissiveBias = 0.35f;
                cfg.raysIntensity = 0.85f;
                cfg.raysDensity = 0.90f;
                cfg.raysDecay = 0.955f;
                cfg.raysWeight = 0.45f;
                cfg.raysSamples = 30;
                cfg.raysOriginX = 0.50f;
                cfg.raysOriginY = 0.50f;
                cfg.raysSun = 0.80f;
                cfg.raysSunSize = 0.14f;
                cfg.raysShimmer = 0.30f;
                cfg.raysOcclusion = 0.40f;
                cfg.exposure = 1.10f;
                cfg.contrast = 1.15f;
                cfg.saturation = 1.35f;
                cfg.temperature = 0.50f;
                cfg.clarity = 0.10f;
                cfg.blackPoint = 0.050f;
                cfg.splitShadow = 0.20f;
                cfg.splitHighlight = 0.60f;
                cfg.vignette = 0.35f;
                cfg.chromatic = 0.20f;
                cfg.halation = 0.60f;
                cfg.grain = 0.015f;
                return;

            // Cheap optics on purpose: fringing, curvature and grain.
            case FRTXPreset::Retro:
                cfg.bgSuppress = 0.35f;
                cfg.bloomIntensity = 1.20f;
                cfg.bloomThreshold = 0.60f;
                cfg.bloomKnee = 0.35f;
                cfg.bloomRadius = 1.6f;
                cfg.bloomSpread = 0.65f;
                cfg.emissiveBias = 0.50f;
                cfg.exposure = 1.0f;
                cfg.contrast = 1.15f;
                cfg.saturation = 1.15f;
                cfg.temperature = 0.12f;
                cfg.clarity = 0.15f;
                cfg.clarityRadius = 3.0f;
                cfg.blackPoint = 0.040f;
                cfg.splitShadow = 0.25f;
                cfg.splitHighlight = 0.25f;
                cfg.vignette = 0.50f;
                cfg.chromatic = 0.55f;
                cfg.lensDistortion = 0.12f;
                cfg.flareIntensity = 0.15f;
                cfg.grain = 0.060f;
                return;

            case FRTXPreset::Count:
                return;

            case FRTXPreset::Subtle:
                cfg.bgSuppress = 0.30f;
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
                cfg.bgSuppress = 0.35f;
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
                cfg.bgSuppress = 0.40f;
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
                cfg.bgSuppress = 0.20f;
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
