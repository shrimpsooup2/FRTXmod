#pragma once

// Plain snapshot of every user setting, read once per frame.
//
// The renderer must never read settings in the middle of the pass chain: the
// capture node and the composite node run at opposite ends of the frame, and if
// the user drags a slider in between the two halves would disagree about how
// many render targets exist. So the capture node takes one snapshot and the
// composite node uses that same snapshot.

enum class FRTXPreset {
    Custom = 0,
    Subtle = 1,
    Showcase = 2,
    Overkill = 3,
};

struct FRTXConfig {
    bool  enabled         = true;

    bool  bloomEnabled    = true;
    float bloomIntensity  = 1.60f;
    float bloomThreshold  = 0.52f;
    float bloomKnee       = 0.45f;
    float bloomRadius     = 2.0f;
    int   bloomLevels     = 3;
    float bloomScale      = 0.5f;
    // How fast the per level bloom weights fall off, and therefore how large
    // the halo around a light source reads. 0 is a tight glow, 1 lets the
    // widest levels through in full.
    float bloomSpread     = 0.8f;
    // Biases the bright pass towards saturated pixels. Neon and glow objects in
    // GD are saturated; skies and background gradients are bright but washed
    // out. This is what stops the whole background from blooming, and it is the
    // closest we can get to object-aware emission without reading object data.
    float emissiveBias    = 0.55f;

    float streakIntensity = 0.0f;
    float streakLength    = 1.4f;

    bool  tonemapEnabled  = true;
    float exposure        = 1.05f;
    float contrast        = 1.10f;
    float saturation      = 1.25f;
    float temperature     = 0.0f;
    float tint            = 0.0f;

    // Unsharp mask against a wide blur of the scene. Reads as depth in the
    // geometry, which is most of what "ray traced" footage is actually selling.
    float clarity         = 0.12f;
    float clarityRadius   = 3.5f;

    float blackPoint      = 0.05f;
    float splitShadow     = 0.20f;
    float splitHighlight  = 0.0f;

    float vignette        = 0.10f;
    float chromatic       = 0.06f;
    float grain           = 0.0f;
    bool  dither          = true;

    int   debugView       = 0;

    static FRTXConfig read();

    // Streaks are built from the bright pass, so they need the bloom chain.
    bool streaksEnabled() const {
        return bloomEnabled && streakIntensity > 0.0f;
    }

    // True when the effect would be a no-op, so we can skip the capture
    // entirely and let the game render straight to the screen.
    bool isNoOp() const;

    // True when the two configs need a different set of render targets.
    bool needsRealloc(FRTXConfig const& other) const;
};
