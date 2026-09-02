#pragma once

// Plain snapshot of every user setting, read once per frame.
//
// The renderer must never re-read settings midway through the pass chain: if a
// value changed between the capture and the composite the two halves would
// disagree about how many render targets exist. One snapshot is taken per
// frame and every pass uses that.
//
// The fields are generated from FRTXParams.inc, which tools/gen_settings.py
// also generates mod.json from, so a setting cannot exist in one and not the
// other.

struct FRTXColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

enum class FRTXPreset {
    Custom = 0,
    Subtle = 1,
    Showcase = 2,
    Overkill = 3,
    Performance = 4,
};

struct FRTXConfig {
#define FRTX_FLOAT(key, member, label, lo, hi, def, step) float member = def;
#define FRTX_INT(key, member, label, lo, hi, def, step)   int member = def;
#define FRTX_BOOL(key, member, label, def)                bool member = def;
#define FRTX_COLOR(key, member, label, r, g, b)           \
    FRTXColor member{r / 255.0f, g / 255.0f, b / 255.0f};
#include "FRTXParams.inc"

    // Reads every setting from Geode. Each lookup is a hash lookup plus a
    // dynamic_cast, so at forty-odd settings this is far too expensive to do
    // per frame -- use current() instead and let the settings listener decide
    // when a re-read is actually needed.
    static FRTXConfig read();

    // The cached snapshot. Re-reads only after invalidate().
    static FRTXConfig const& current();

    // Writes a preset's values into the settings, once. Presets deliberately
    // do NOT override anything at read time: an override layer means dragging
    // a slider silently does nothing, which is far worse than no presets.
    // System settings the presets do not mention keep their current values.
    static void applyPreset(FRTXPreset preset);

    // Called by the settings-changed listener. Geode dispatches that event
    // synchronously from setValue, so a write through either the settings menu
    // or the tuner has already invalidated this by the time it returns.
    static void invalidate();

    // Bumped on every invalidate, so derived values computed from a config can
    // be cached and recomputed only when it actually changed.
    static unsigned generation();

    // Streaks and rays are both built from the bright pass, so they need the
    // bloom chain to exist.
    bool streaksEnabled() const { return bloomEnabled && streakIntensity > 0.0f; }
    bool raysEnabled() const { return bloomEnabled && raysIntensity > 0.0f; }

    // True when the effect would be a no-op, so we can skip the capture
    // entirely and let the game render straight to the screen.
    bool isNoOp() const;

    // True when the two configs need a different set of render targets.
    bool needsRealloc(FRTXConfig const& other) const;
};
