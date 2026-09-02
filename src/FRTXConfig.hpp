#pragma once

// Plain snapshot of every user setting, read once per frame.
//
// The renderer must never read settings in the middle of the pass chain: the
// capture node and the composite node run at opposite ends of the frame, and if
// the user drags a slider in between the two halves would disagree about how
// many render targets exist. So the capture node takes one snapshot and the
// composite node uses that same snapshot.
struct FRTXConfig {
    bool  enabled         = true;

    bool  bloomEnabled    = true;
    float bloomIntensity  = 1.0f;
    float bloomThreshold  = 0.75f;
    float bloomKnee       = 0.4f;
    float bloomRadius     = 1.0f;
    int   bloomLevels     = 3;
    float bloomScale      = 0.5f;

    bool  tonemapEnabled  = true;
    float exposure        = 1.0f;
    float contrast        = 1.05f;
    float saturation      = 1.08f;
    float temperature     = 0.15f;
    float tint            = 0.0f;

    float vignette        = 0.25f;
    float chromatic       = 0.12f;
    float grain           = 0.0f;
    bool  dither          = true;

    int   debugView       = 0;

    static FRTXConfig read();

    // True when the effect would be a no-op, so we can skip the capture
    // entirely and let the game render straight to the screen.
    bool isNoOp() const;

    // True when the two configs need a different set of render targets.
    bool needsRealloc(FRTXConfig const& other) const;
};
