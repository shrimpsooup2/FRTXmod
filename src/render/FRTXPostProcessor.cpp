#include "FRTXPostProcessor.hpp"

#include "FRTXShaders.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace frtx {

namespace {
    char const* const kBloomUVNames[PostProcessor::kMaxBloomLevels] = {
        "u_bloomUV0", "u_bloomUV1", "u_bloomUV2",
    };

    // Every pass overwrites its whole target, so replace rather than blend.
    void setReplaceBlend() {
        ccGLBlendFunc(GL_ONE, GL_ZERO);
    }
}

PostProcessor& PostProcessor::get() {
    static PostProcessor instance;
    return instance;
}

bool PostProcessor::beginCapture() {
    if (m_capturing) {
        // The composite node never ran: it was removed from the scene, or the
        // scene changed, in the middle of a frame. Close the target now so the
        // game does not keep rendering into a buffer nobody displays.
        log::warn("capture was still open at the start of a frame, closing it");
        if (m_scene.valid()) m_scene.rt->end();
        m_capturing = false;
    }

    auto cfg = FRTXConfig::read();
    if (cfg.isNoOp()) return false;

    if (!ensurePrograms()) return false;
    if (!ensureTargets(cfg)) return false;

    m_frameConfig = cfg;
    m_scene.rt->beginWithClear(0.0f, 0.0f, 0.0f, 0.0f);
    m_capturing = true;
    return true;
}

void PostProcessor::endCapture() {
    if (!m_capturing) return;
    m_capturing = false;

    if (!m_scene.valid()) return;
    m_scene.rt->end();

    m_time += CCDirector::sharedDirector()->getDeltaTime();
    if (m_time > 3600.0f) m_time = 0.0f;

    auto const& cfg = m_frameConfig;
    if (cfg.bloomEnabled && m_activeLevels > 0) {
        buildBloom(cfg);
    }
    present(cfg);
}

void PostProcessor::releaseResources() {
    if (m_capturing) {
        if (m_scene.valid()) m_scene.rt->end();
        m_capturing = false;
    }
    releaseTargets();
}

bool PostProcessor::ensurePrograms() {
    if (m_programsFailed) return false;
    if (m_composite.valid()) return true;

    bool const ok =
        m_prefilter.init("prefilter", shaders::PREFILTER) &&
        m_downsample.init("downsample", shaders::DOWNSAMPLE) &&
        m_blur.init("blur", shaders::BLUR) &&
        m_composite.init("composite", shaders::COMPOSITE);

    if (!ok) {
        log::error("shader setup failed; FRTX will stay inactive for this session");
        m_prefilter.destroy();
        m_downsample.destroy();
        m_blur.destroy();
        m_composite.destroy();
        m_programsFailed = true;
        return false;
    }

    log::info("shaders compiled");
    return true;
}

bool PostProcessor::ensureTargets(FRTXConfig const& cfg) {
    auto director = CCDirector::sharedDirector();
    auto const winSize = director->getWinSize();
    auto const scaleFactor = CC_CONTENT_SCALE_FACTOR();

    bool const rebuild =
        !m_targetsValid ||
        winSize.width != m_targetWidth ||
        winSize.height != m_targetHeight ||
        scaleFactor != m_targetScaleFactor ||
        cfg.needsRealloc(m_targetConfig);

    if (!rebuild) return true;

    releaseTargets();

    int const sceneW = static_cast<int>(std::ceil(winSize.width));
    int const sceneH = static_cast<int>(std::ceil(winSize.height));
    if (sceneW < 4 || sceneH < 4) return false;

    // The capture target must stay the same size as the screen. CCRenderTexture
    // rewrites the projection to fit whatever size it is given, so a downscaled
    // capture would rescale the scene rather than sample it more coarsely. Only
    // the bloom pyramid is allowed to shrink.
    if (!m_scene.create(sceneW, sceneH)) return false;

    // Rounding up to whole points leaves a sliver of the target unused; work out
    // how much so every pass samples exactly what the game drew.
    auto const pixels = director->getWinSizeInPixels();
    auto const contentW = static_cast<float>(static_cast<int>(sceneW * scaleFactor));
    auto const contentH = static_cast<float>(static_cast<int>(sceneH * scaleFactor));
    m_sceneFillX = contentW > 0.0f ? std::min(1.0f, pixels.width / contentW) : 1.0f;
    m_sceneFillY = contentH > 0.0f ? std::min(1.0f, pixels.height / contentH) : 1.0f;

    m_activeLevels = 0;
    if (cfg.bloomEnabled) {
        int const levels = std::clamp(cfg.bloomLevels, 1, kMaxBloomLevels);
        int w = static_cast<int>(sceneW * cfg.bloomScale);
        int h = static_cast<int>(sceneH * cfg.bloomScale);

        for (int i = 0; i < levels; ++i) {
            if (w < 4 || h < 4) break;
            if (!m_bloom[i].create(w, h) || !m_bloomTemp[i].create(w, h)) {
                log::warn("could not allocate bloom level {}, stopping the chain there", i);
                m_bloom[i].destroy();
                m_bloomTemp[i].destroy();
                break;
            }
            ++m_activeLevels;
            w /= 2;
            h /= 2;
        }

        if (m_activeLevels == 0) {
            log::warn("bloom is enabled but no levels could be allocated");
        }
    }

    m_targetsValid = true;
    m_targetConfig = cfg;
    m_targetWidth = winSize.width;
    m_targetHeight = winSize.height;
    m_targetScaleFactor = scaleFactor;
    m_aspect = winSize.height > 0.0f ? winSize.width / winSize.height : 1.0f;

    log::info("allocated a {}x{} capture with {} bloom level(s)", sceneW, sceneH, m_activeLevels);
    return true;
}

void PostProcessor::releaseTargets() {
    m_scene.destroy();
    for (int i = 0; i < kMaxBloomLevels; ++i) {
        m_bloom[i].destroy();
        m_bloomTemp[i].destroy();
    }
    m_activeLevels = 0;
    m_targetsValid = false;
}

void PostProcessor::buildBloom(FRTXConfig const& cfg) {
    float const knee = std::max(cfg.bloomKnee, 0.0001f);

    // Level 0 is the bright pass, taken straight off the captured scene.
    {
        auto& dst = m_bloom[0];
        dst.rt->begin();
        m_prefilter.use();
        m_prefilter.set1i("u_source", 0);
        m_prefilter.set2f("u_sourceUV", m_scene.uvW * m_sceneFillX, m_scene.uvH * m_sceneFillY);
        m_prefilter.set2f("u_texelSize", m_scene.texelW, m_scene.texelH);
        m_prefilter.set3f("u_filter", cfg.bloomThreshold, knee, 1.0f / (4.0f * knee));
        bindTexture(0, m_scene.textureName());
        setReplaceBlend();
        drawFullscreenQuad();
        dst.rt->end();
    }

    // Each lower level is a half-resolution box downsample of the one above.
    for (int i = 1; i < m_activeLevels; ++i) {
        auto& src = m_bloom[i - 1];
        auto& dst = m_bloom[i];
        dst.rt->begin();
        m_downsample.use();
        m_downsample.set1i("u_source", 0);
        m_downsample.set2f("u_sourceUV", src.uvW, src.uvH);
        m_downsample.set2f("u_texelSize", src.texelW, src.texelH);
        bindTexture(0, src.textureName());
        setReplaceBlend();
        drawFullscreenQuad();
        dst.rt->end();
    }

    for (int i = 0; i < m_activeLevels; ++i) {
        blurLevel(m_bloom[i], m_bloomTemp[i], cfg.bloomRadius);
    }
}

void PostProcessor::blurLevel(Target& target, Target& temp, float radius) {
    // Horizontal into the scratch buffer...
    temp.rt->begin();
    m_blur.use();
    m_blur.set1i("u_source", 0);
    m_blur.set2f("u_sourceUV", target.uvW, target.uvH);
    m_blur.set2f("u_offset", target.texelW * radius, 0.0f);
    bindTexture(0, target.textureName());
    setReplaceBlend();
    drawFullscreenQuad();
    temp.rt->end();

    // ...then vertical, back into the level itself.
    target.rt->begin();
    m_blur.use();
    m_blur.set1i("u_source", 0);
    m_blur.set2f("u_sourceUV", temp.uvW, temp.uvH);
    m_blur.set2f("u_offset", 0.0f, temp.texelH * radius);
    bindTexture(0, temp.textureName());
    setReplaceBlend();
    drawFullscreenQuad();
    target.rt->end();
}

void PostProcessor::present(FRTXConfig const& cfg) {
    // Weights fall off per level so the tight glow dominates and the wider
    // levels only add atmosphere. Normalising them keeps "intensity" meaning the
    // same thing whatever the level count is.
    float weights[kMaxBloomLevels] = {0.0f, 0.0f, 0.0f};
    if (cfg.bloomEnabled && m_activeLevels > 0) {
        float sum = 0.0f;
        for (int i = 0; i < m_activeLevels; ++i) {
            weights[i] = std::pow(0.75f, static_cast<float>(i));
            sum += weights[i];
        }
        for (int i = 0; i < m_activeLevels; ++i) weights[i] /= sum;
    }

    m_composite.use();
    m_composite.set1i("u_scene", 0);
    m_composite.set1i("u_bloom0", 1);
    m_composite.set1i("u_bloom1", 2);
    m_composite.set1i("u_bloom2", 3);
    m_composite.set2f("u_sceneUV", m_scene.uvW * m_sceneFillX, m_scene.uvH * m_sceneFillY);

    for (int i = 0; i < kMaxBloomLevels; ++i) {
        // Sampling a texture unit with nothing bound is undefined, so unused
        // bloom slots point at the scene and are multiplied by a zero weight.
        bool const used = i < m_activeLevels && weights[i] > 0.0f;
        Target const& source = used ? m_bloom[i] : m_scene;
        m_composite.set2f(kBloomUVNames[i], source.uvW, source.uvH);
        bindTexture(i + 1, source.textureName());
    }

    // Bind unit 0 last so cocos gets the texture unit back the way it expects.
    bindTexture(0, m_scene.textureName());

    m_composite.set4f("u_bloom", weights[0], weights[1], weights[2],
        cfg.bloomEnabled ? cfg.bloomIntensity : 0.0f);
    m_composite.set4f("u_tone", cfg.exposure, cfg.contrast, cfg.saturation,
        cfg.tonemapEnabled ? 1.0f : 0.0f);
    m_composite.set4f("u_lens", cfg.vignette, cfg.chromatic, cfg.grain,
        cfg.dither ? 1.0f : 0.0f);
    m_composite.set4f("u_misc", m_aspect, m_time, static_cast<float>(cfg.debugView), 0.0f);
    m_composite.set2f("u_grade", cfg.temperature, cfg.tint);

    setReplaceBlend();
    drawFullscreenQuad();

    // Hand cocos back the blend mode it expects for ordinary sprites.
    ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);
}

}
