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
    float bloomIntensity  = 1.45f;
    float bloomThreshold  = 0.62f;
    float bloomKnee       = 0.35f;
    float bloomRadius     = 1.5f;
    int   bloomLevels     = 3;
    float bloomScale      = 0.5f;
    // Biases the bright pass towards saturated pixels. Neon and glow objects in
    // GD are saturated; skies and background gradients are bright but washed
    // out. This is what stops the whole background from blooming, and it is the
    // closest we can get to object-aware emission without reading object data.
    float emissiveBias    = 0.65f;

    float streakIntensity = 0.45f;
    float streakLength    = 1.4f;

    bool  tonemapEnabled  = true;
    float exposure        = 1.08f;
    float contrast        = 1.12f;
    float saturation      = 1.22f;
    float temperature     = 0.10f;
    float tint            = 0.0f;

    // Unsharp mask against a wide blur of the scene. Reads as depth in the
    // geometry, which is most of what "ray traced" footage is actually selling.
    float clarity         = 0.40f;
    float clarityRadius   = 6.0f;

    float blackPoint      = 0.035f;
    float splitShadow     = 0.40f;
    float splitHighlight  = 0.35f;

    float vignette        = 0.35f;
    float chromatic       = 0.18f;
    float grain           = 0.015f;
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
