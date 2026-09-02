#pragma once

#include "../FRTXConfig.hpp"
#include "FRTXGL.hpp"

namespace frtx {

// Owns every GPU resource and runs the pass chain.
//
// The frame is bracketed by two calls that happen at opposite ends of the
// scene graph:
//
//   beginCapture()  <- capture node, z-order INT_MIN, runs before anything else
//       ... the entire game renders into our capture target ...
//   endCapture()    <- composite node, z-order INT_MAX, runs after everything
//
// endCapture() closes the capture target, builds the bloom pyramid and then
// draws the graded result to whatever framebuffer was bound when we started.
class PostProcessor {
public:
    static constexpr int kMaxBloomLevels = 3;

    static PostProcessor& get();

    // Returns true when the frame is now being captured. When it returns false
    // the game renders straight to the screen and endCapture() is a no-op.
    bool beginCapture();
    void endCapture();

    bool isCapturing() const { return m_capturing; }

    // Frees the render targets. Shader programs are kept, they are cheap and
    // recompiling them on every level entry would be wasteful.
    void releaseResources();

private:
    bool ensurePrograms();
    bool ensureTargets(FRTXConfig const& cfg);
    void releaseTargets();

    void buildBloom(FRTXConfig const& cfg);
    void blurPass(Target& dst, Target const& src, float offsetX, float offsetY);
    void blurLevel(Target& target, Target& temp, float radius);
    void buildStreaks(FRTXConfig const& cfg);
    void present(FRTXConfig const& cfg);

    bool m_capturing = false;
    bool m_programsFailed = false;

    // Snapshot taken in beginCapture() and used by endCapture(), so both halves
    // of the frame always agree even if a setting changes in between.
    FRTXConfig m_frameConfig;

    bool  m_targetsValid = false;
    FRTXConfig m_targetConfig;
    float m_targetWidth = 0.0f;
    float m_targetHeight = 0.0f;
    float m_targetScaleFactor = 0.0f;
    int   m_activeLevels = 0;

    Target m_scene;
    Target m_bloom[kMaxBloomLevels];
    Target m_bloomTemp[kMaxBloomLevels];

    // Ping-pong pair for the anamorphic streak, which is three horizontal-only
    // blurs of the bright pass with rapidly growing steps. The finished streak
    // always ends up in m_streak[0].
    Target m_streak[2];
    bool m_streakValid = false;

    // Fraction of the capture target the game actually draws into, because the
    // target is rounded up to whole points.
    float m_sceneFillX = 1.0f;
    float m_sceneFillY = 1.0f;

    Program m_prefilter;
    Program m_downsample;
    Program m_blur;
    Program m_composite;

    float m_time = 0.0f;
    float m_aspect = 1.0f;
    float m_pixelHeight = 1.0f;
};

}
