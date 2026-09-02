#pragma once

#include "../FRTXConfig.hpp"
#include "FRTXGL.hpp"

namespace frtx {

// Anything the renderer needs from the game that it cannot work out itself.
struct FrameInfo {
    bool  hasPlayer = false;
    float playerX = 0.5f;  // normalised screen space
    float playerY = 0.5f;
};

// Owns every GPU resource and runs the pass chain.
//
// The frame is bracketed from inside GJBaseGameLayer::visit():
//
//   beginCapture()      binds the capture target
//       GJBaseGameLayer::visit()   -- the whole game renders into it
//   endCapture()        builds the bloom pyramid and composites to the screen
//
// Wrapping the game layer's own visit() rather than the scene means the
// begin/end pair lives in one function, so the matrix stack and framebuffer
// binding are trivially balanced, and it covers PlayLayer and LevelEditorLayer
// alike since both inherit that method.
class PostProcessor {
public:
    static constexpr int kMaxBloomLevels = 3;

    static PostProcessor& get();

    // Returns true when the frame is now being captured. When it returns false
    // the game renders straight to the screen and endCapture() is a no-op.
    bool beginCapture(FrameInfo const& info);
    void endCapture();

    // Compiles the shaders and allocates the render targets ahead of time.
    // Called when a level opens: doing it lazily on the first captured frame
    // puts five shader compiles and eight framebuffer allocations inside the
    // first frame the player sees, which is exactly where a hitch is most
    // noticeable.
    void warmUp();

    bool isCapturing() const { return m_capturing; }

    // Frees the render targets. Shader programs are kept, they are cheap and
    // recompiling them on every level entry would be wasteful.
    void releaseResources();

    FRTXConfig const& frameConfig() const { return m_frameConfig; }

private:
    bool ensurePrograms();
    bool ensureTargets(FRTXConfig const& cfg);
    void releaseTargets();

    void buildBloom(FRTXConfig const& cfg);
    void blurPass(Target& dst, Target const& src, float offsetX, float offsetY);
    void blurLevel(Target& target, Target& temp, float radius);
    void buildStreaks(FRTXConfig const& cfg);
    void buildRays(FRTXConfig const& cfg);
    void present(FRTXConfig const& cfg);

    bool m_capturing = false;
    bool m_programsFailed = false;

    // Snapshot taken in beginCapture() and used by endCapture(), so both halves
    // of the frame always agree even if a setting changes in between.
    FRTXConfig m_frameConfig;
    FrameInfo m_frameInfo;

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

    Target m_rays;
    bool m_raysValid = false;

    // Fraction of the capture target the game actually draws into, because the
    // target is rounded up to whole points.
    float m_sceneFillX = 1.0f;
    float m_sceneFillY = 1.0f;

    Program m_prefilter;
    Program m_downsample;
    Program m_blur;
    Program m_rayProgram;
    Program m_composite;

    // Per level bloom weights, recomputed only when the settings actually
    // change rather than every frame.
    float m_weights[kMaxBloomLevels] = {0.0f, 0.0f, 0.0f};
    unsigned m_weightsStamp = 0;

    float m_time = 0.0f;
    float m_aspect = 1.0f;
    float m_pixelHeight = 1.0f;
};

}
