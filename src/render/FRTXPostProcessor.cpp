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

bool PostProcessor::beginCapture(FrameInfo const& info) {
    if (m_capturing) {
        // The composite node never ran: it was removed from the scene, or the
        // scene changed, in the middle of a frame. Close the target now so the
        // game does not keep rendering into a buffer nobody displays.
        log::warn("capture was still open at the start of a frame, closing it");
        restoreGLState(m_savedState);
        m_capturing = false;
    }

    auto const& cfg = FRTXConfig::current();
    if (cfg.isNoOp()) return false;

    if (!ensurePrograms()) return false;

    // Size the capture from the viewport the game is actually rendering to,
    // not from the design resolution. They are normally the same, but the
    // viewport is the thing that decides what gets clipped, and it is what the
    // game's own camera handling can change underneath us.
    auto const state = captureGLState();
    if (state.viewport[2] <= 0 || state.viewport[3] <= 0) return false;
    if (!ensureTargets(cfg, state.viewport[2], state.viewport[3])) return false;

    m_frameConfig = cfg;
    m_frameInfo = info;
    m_savedState = state;

    // Bind the capture target and clear it, and touch nothing else. The game's
    // projection, modelview and viewport are whatever its own camera effects
    // have set up, and the capture is the same size as the screen, so they are
    // all still exactly right. CCRenderTexture::begin() would instead reload
    // the standard projection and reset the viewport, which is what made the
    // effect fight camera triggers.
    glBindFramebuffer(GL_FRAMEBUFFER, m_scene.fbo);

    // The clear must not inherit the game's scissor box or colour mask. A
    // scissor left enabled would wipe only part of the capture and leave the
    // rest holding the previous frame, which then gets bloomed on top of this
    // one -- the effect appearing to grow stronger the longer you look at it.
    suspendClipping();

    GLfloat clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    // Hand clipping straight back, so the game's own drawing is unaffected.
    restoreClipping(m_savedState);

    m_capturing = true;
    return true;
}

void PostProcessor::endCapture() {
    if (!m_capturing) return;
    m_capturing = false;

    if (!m_scene.valid()) {
        restoreGLState(m_savedState);
        return;
    }

    // Every pass below writes a full target, so none of them may be clipped by
    // whatever the game had set up.
    suspendClipping();

    m_time += CCDirector::sharedDirector()->getDeltaTime();
    if (m_time > 3600.0f) m_time = 0.0f;

    auto const& cfg = m_frameConfig;
    if (cfg.bloomEnabled && m_activeLevels > 0) {
        buildBloom(cfg);
        if (cfg.streaksEnabled() && m_streakValid) {
            buildStreaks(cfg);
        }
        if (cfg.raysEnabled() && m_raysValid) {
            buildRays(cfg);
        }
    }
    present(cfg);
}

void PostProcessor::warmUp() {
    auto const& cfg = FRTXConfig::current();
    if (cfg.isNoOp()) return;
    if (!ensurePrograms()) return;
    auto const state = captureGLState();
    if (state.viewport[2] > 0 && state.viewport[3] > 0) {
        ensureTargets(cfg, state.viewport[2], state.viewport[3]);
    }
}

void PostProcessor::releaseResources() {
    if (m_capturing) {
        restoreGLState(m_savedState);
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
        m_rayProgram.init("rays", shaders::RAYS) &&
        m_composite.init("composite", shaders::COMPOSITE);

    if (!ok) {
        log::error("shader setup failed; FRTX will stay inactive for this session");
        m_prefilter.destroy();
        m_downsample.destroy();
        m_blur.destroy();
        m_rayProgram.destroy();
        m_composite.destroy();
        m_programsFailed = true;
        return false;
    }

    log::info("shaders compiled");
    return true;
}

bool PostProcessor::ensureTargets(FRTXConfig const& cfg, int viewportW, int viewportH) {
    auto director = CCDirector::sharedDirector();
    auto const winSize = director->getWinSize();
    auto const scaleFactor = std::max(CC_CONTENT_SCALE_FACTOR(), 0.0001f);

    bool const rebuild =
        !m_targetsValid ||
        viewportW != m_targetViewportW ||
        viewportH != m_targetViewportH ||
        scaleFactor != m_targetScaleFactor ||
        cfg.needsRealloc(m_targetConfig);

    if (!rebuild) return true;

    releaseTargets();

    // CCRenderTexture allocates in points and multiplies by the content scale
    // factor, so convert the viewport's pixels back into points and round up:
    // the target must never be smaller than the viewport or the scene would be
    // clipped rather than captured.
    int const sceneW = static_cast<int>(std::ceil(viewportW / scaleFactor));
    int const sceneH = static_cast<int>(std::ceil(viewportH / scaleFactor));
    if (sceneW < 4 || sceneH < 4) return false;

    // The capture target must stay the same size as the screen. CCRenderTexture
    // rewrites the projection to fit whatever size it is given, so a downscaled
    // capture would rescale the scene rather than sample it more coarsely. Only
    // the bloom pyramid is allowed to shrink.
    if (!m_scene.create(sceneW, sceneH)) return false;

    // Rounding up to whole points leaves a sliver of the target unused; work
    // out how much so every pass samples exactly the region the game drew into.
    m_sceneFillX = m_scene.pixelWidth > 0
        ? std::min(1.0f, static_cast<float>(viewportW) / static_cast<float>(m_scene.pixelWidth))
        : 1.0f;
    m_sceneFillY = m_scene.pixelHeight > 0
        ? std::min(1.0f, static_cast<float>(viewportH) / static_cast<float>(m_scene.pixelHeight))
        : 1.0f;

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

    m_streakValid = false;
    if (cfg.streaksEnabled() && m_activeLevels > 0) {
        // Half of the first bloom level again. Streaks are wide and soft, so
        // resolution buys nothing here.
        int const sw = std::max(4, m_bloom[0].widthPoints / 2);
        int const sh = std::max(4, m_bloom[0].heightPoints / 2);
        if (m_streak[0].create(sw, sh) && m_streak[1].create(sw, sh)) {
            m_streakValid = true;
        } else {
            log::warn("could not allocate the streak buffers, streaks are off");
            m_streak[0].destroy();
            m_streak[1].destroy();
        }
    }

    m_raysValid = false;
    if (cfg.raysEnabled() && m_activeLevels > 0) {
        int const rw = std::max(4, m_bloom[0].widthPoints / 2);
        int const rh = std::max(4, m_bloom[0].heightPoints / 2);
        if (m_rays.create(rw, rh)) {
            m_raysValid = true;
        } else {
            log::warn("could not allocate the light ray buffer, rays are off");
            m_rays.destroy();
        }
    }

    m_targetsValid = true;
    m_targetConfig = cfg;
    m_targetViewportW = viewportW;
    m_targetViewportH = viewportH;
    m_targetScaleFactor = scaleFactor;
    m_aspect = winSize.height > 0.0f ? winSize.width / winSize.height : 1.0f;
    m_pixelHeight = viewportH > 1 ? static_cast<float>(viewportH) : 1.0f;

    log::info("allocated a {}x{}px capture for a {}x{}px viewport with {} bloom level(s)",
        m_scene.pixelWidth, m_scene.pixelHeight, viewportW, viewportH, m_activeLevels);
    return true;
}

void PostProcessor::releaseTargets() {
    m_scene.destroy();
    for (int i = 0; i < kMaxBloomLevels; ++i) {
        m_bloom[i].destroy();
        m_bloomTemp[i].destroy();
    }
    m_streak[0].destroy();
    m_streak[1].destroy();
    m_streakValid = false;
    m_rays.destroy();
    m_raysValid = false;
    m_activeLevels = 0;
    m_targetsValid = false;
    m_weightsStamp = 0;
}

void PostProcessor::buildBloom(FRTXConfig const& cfg) {
    float const knee = std::max(cfg.bloomKnee, 0.0001f);

    // Level 0 is the bright pass, taken straight off the captured scene.
    {
        auto& dst = m_bloom[0];
        bindTargetForDrawing(dst);
        m_prefilter.use();
        m_prefilter.set1i("u_source", 0);
        m_prefilter.set2f("u_sourceUV", m_scene.uvW * m_sceneFillX, m_scene.uvH * m_sceneFillY);
        m_prefilter.set2f("u_texelSize", m_scene.texelW, m_scene.texelH);
        m_prefilter.set4f("u_filter", cfg.bloomThreshold, knee, 1.0f / (4.0f * knee),
            cfg.emissiveBias);
        // The radius is a fraction of screen height, so the horizontal offset is
        // divided by the aspect ratio to sample a square neighbourhood.
        m_prefilter.set4f("u_filter2", cfg.bgSuppress,
            cfg.bgRadius / std::max(m_aspect, 0.0001f), cfg.bgRadius, cfg.isolation);
        bindTexture(0, m_scene.textureName());
        setReplaceBlend();
        drawFullscreenQuad();
    }

    // Each lower level is a half-resolution box downsample of the one above.
    for (int i = 1; i < m_activeLevels; ++i) {
        auto& src = m_bloom[i - 1];
        auto& dst = m_bloom[i];
        bindTargetForDrawing(dst);
        m_downsample.use();
        m_downsample.set1i("u_source", 0);
        m_downsample.set2f("u_sourceUV", src.uvW, src.uvH);
        m_downsample.set2f("u_texelSize", src.texelW, src.texelH);
        bindTexture(0, src.textureName());
        setReplaceBlend();
        drawFullscreenQuad();
    }

    for (int i = 0; i < m_activeLevels; ++i) {
        blurLevel(m_bloom[i], m_bloomTemp[i], cfg.bloomRadius);
    }
}

void PostProcessor::blurPass(Target& dst, Target const& src, float offsetX, float offsetY) {
    bindTargetForDrawing(dst);
    m_blur.use();
    m_blur.set1i("u_source", 0);
    m_blur.set2f("u_sourceUV", src.uvW, src.uvH);
    m_blur.set2f("u_offset", offsetX, offsetY);
    bindTexture(0, src.textureName());
    setReplaceBlend();
    drawFullscreenQuad();
}

void PostProcessor::blurLevel(Target& target, Target& temp, float radius) {
    blurPass(temp, target, target.texelW * radius, 0.0f);
    blurPass(target, temp, 0.0f, temp.texelH * radius);
}

void PostProcessor::buildStreaks(FRTXConfig const& cfg) {
    // Three horizontal passes with the step growing by 4x each time. Chaining
    // them this way reaches roughly +/-70 texels of smear for nine texture
    // fetches per pixel, which a single wide blur could not touch.
    float const length = cfg.streakLength;

    blurPass(m_streak[0], m_bloom[0], m_bloom[0].texelW * length, 0.0f);
    blurPass(m_streak[1], m_streak[0], m_streak[0].texelW * length * 4.0f, 0.0f);
    blurPass(m_streak[0], m_streak[1], m_streak[1].texelW * length * 16.0f, 0.0f);
}

void PostProcessor::buildRays(FRTXConfig const& cfg) {
    float originX = cfg.raysOriginX;
    float originY = cfg.raysOriginY;
    if (cfg.raysOriginMode == 1 && m_frameInfo.hasPlayer) {
        originX = m_frameInfo.playerX;
        originY = m_frameInfo.playerY;
    }

    // Normalise by the sum the march actually accumulates -- a geometric
    // series in the decay -- rather than by a guess. That keeps brightness
    // stable while sample count and decay are dragged around, so those stay
    // shape controls and intensity is the only thing that sets how bright the
    // shafts are. The previous approximation over-divided badly at high sample
    // counts, which is why rays came out weak.
    float const samples = static_cast<float>(cfg.raysSamples);
    float const decay = std::clamp(cfg.raysDecay, 0.0f, 0.9999f);
    float const series = (1.0f - std::pow(decay, samples)) / std::max(1.0f - decay, 1e-4f);
    float const normalisation = 1.0f / std::max(cfg.raysWeight * series, 1e-3f);

    bindTargetForDrawing(m_rays);
    m_rayProgram.use();
    m_rayProgram.set1i("u_source", 0);
    m_rayProgram.set2f("u_sourceUV", m_bloom[0].uvW, m_bloom[0].uvH);
    m_rayProgram.set2f("u_origin", originX, originY);
    m_rayProgram.set4f("u_params", cfg.raysDensity, cfg.raysDecay, cfg.raysWeight, normalisation);
    m_rayProgram.set4f("u_ray2", cfg.raysJitter, cfg.raysSun, cfg.raysSunSize, cfg.raysShimmer);
    m_rayProgram.set2f("u_rayMisc", m_aspect, m_time);
    m_rayProgram.set1f("u_samples", samples);
    bindTexture(0, m_bloom[0].textureName());
    setReplaceBlend();
    drawFullscreenQuad();
}

void PostProcessor::present(FRTXConfig const& cfg) {
    // How fast the per level weights fall off decides how big the halo reads.
    // A steep falloff keeps the glow tight around the object; a flat one lets
    // the widest levels through and gives the large soft halo that showcase
    // footage has around every light source. Normalising keeps "intensity"
    // meaning the same thing whatever the level count is.
    //
    // Recomputed only when the settings change, not per frame: three pow()
    // calls are not much, but nothing here needs doing sixty times a second.
    if (m_weightsStamp != FRTXConfig::generation()) {
        for (int i = 0; i < kMaxBloomLevels; ++i) m_weights[i] = 0.0f;
        if (cfg.bloomEnabled && m_activeLevels > 0) {
            float const falloff = 0.5f + 0.5f * cfg.bloomSpread;
            float sum = 0.0f;
            for (int i = 0; i < m_activeLevels; ++i) {
                m_weights[i] = std::pow(falloff, static_cast<float>(i));
                sum += m_weights[i];
            }
            for (int i = 0; i < m_activeLevels; ++i) m_weights[i] /= sum;
        }
        m_weightsStamp = FRTXConfig::generation();
    }
    float const* const weights = m_weights;

    m_composite.use();
    m_composite.set1i("u_scene", 0);
    m_composite.set1i("u_bloom0", 1);
    m_composite.set1i("u_bloom1", 2);
    m_composite.set1i("u_bloom2", 3);
    m_composite.set1i("u_streakTex", 4);
    m_composite.set1i("u_raysTex", 5);
    m_composite.set2f("u_sceneUV", m_scene.uvW * m_sceneFillX, m_scene.uvH * m_sceneFillY);

    for (int i = 0; i < kMaxBloomLevels; ++i) {
        // Sampling a texture unit with nothing bound is undefined, so unused
        // slots point at the scene and are multiplied by a zero weight.
        bool const used = i < m_activeLevels && weights[i] > 0.0f;
        Target const& source = used ? m_bloom[i] : m_scene;
        m_composite.set2f(kBloomUVNames[i], source.uvW, source.uvH);
        bindTexture(i + 1, source.textureName());
    }

    bool const streaks = cfg.streaksEnabled() && m_streakValid;
    Target const& streakSource = streaks ? m_streak[0] : m_scene;
    m_composite.set2f("u_streakUV", streakSource.uvW, streakSource.uvH);
    bindTexture(4, streakSource.textureName());

    bool const rays = cfg.raysEnabled() && m_raysValid;
    Target const& raySource = rays ? m_rays : m_scene;
    m_composite.set2f("u_raysUV", raySource.uvW, raySource.uvH);
    bindTexture(5, raySource.textureName());

    // Halation wants the widest blur available, which is not necessarily level
    // 2: the level count is a setting.
    Target const& halationSource =
        (cfg.bloomEnabled && m_activeLevels > 0) ? m_bloom[m_activeLevels - 1] : m_scene;
    m_composite.set1i("u_halationTex", 6);
    m_composite.set2f("u_halationUV", halationSource.uvW, halationSource.uvH);
    bindTexture(6, halationSource.textureName());

    // Bind unit 0 last so cocos gets the texture unit back the way it expects.
    bindTexture(0, m_scene.textureName());

    float const streakAmount = streaks ? cfg.streakIntensity : 0.0f;
    m_composite.set3f("u_streak",
        cfg.streakTint.r * streakAmount,
        cfg.streakTint.g * streakAmount,
        cfg.streakTint.b * streakAmount);

    float const rayAmount = rays ? cfg.raysIntensity : 0.0f;
    m_composite.set3f("u_rays",
        cfg.raysTint.r * rayAmount,
        cfg.raysTint.g * rayAmount,
        cfg.raysTint.b * rayAmount);

    m_composite.set4f("u_bloom", weights[0], weights[1], weights[2],
        cfg.bloomEnabled ? cfg.bloomIntensity : 0.0f);
    m_composite.set3f("u_bloomTint", cfg.bloomTint.r, cfg.bloomTint.g, cfg.bloomTint.b);
    m_composite.set4f("u_tone", cfg.exposure, cfg.contrast, cfg.saturation,
        cfg.tonemapEnabled ? 1.0f : 0.0f);
    m_composite.set4f("u_lens", cfg.chromatic, cfg.grain, cfg.dither ? 1.0f : 0.0f, 0.0f);

    // Softness widens the band the darkening fades across, anchored at the
    // corners so raising it eats further into the frame rather than moving the
    // edge of the effect.
    float const outer = 0.9f;
    float const inner = outer - (0.15f + 0.6f * cfg.vignetteSoftness);
    m_composite.set4f("u_vignette", cfg.vignette, cfg.vignetteRoundness, inner, outer);

    m_composite.set4f("u_flare", cfg.flareIntensity, cfg.flareSpacing, cfg.halation,
        cfg.raysEnabled() ? cfg.raysFadeOverDark : 0.0f);
    m_composite.set2f("u_lens2", cfg.lensDistortion, 0.0f);
    m_composite.set4f("u_misc", m_aspect, m_time, static_cast<float>(cfg.debugView), 0.0f);
    m_composite.set2f("u_grade", cfg.temperature, cfg.tint);
    m_composite.set4f("u_grade2", cfg.blackPoint, cfg.splitShadow, cfg.splitHighlight, 0.0f);
    // 0.25 is enough headroom for real detail while staying well short of the
    // contrast that would show as a ring.
    m_composite.set4f("u_clarity", cfg.clarity, cfg.clarityRadius / m_pixelHeight, 0.25f,
        cfg.clarityTaps > 4 ? 1.0f : 0.0f);

    // Back to the game's own framebuffer and viewport, exactly as they were.
    restoreGLState(m_savedState);

    setReplaceBlend();
    drawFullscreenQuad();

    // Hand cocos back the blend mode and clipping it expects.
    ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);
    restoreClipping(m_savedState);
}

}
