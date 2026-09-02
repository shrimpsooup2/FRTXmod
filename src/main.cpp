#include "FRTXConfig.hpp"
#include "render/FRTXPostProcessor.hpp"
#include "ui/FRTXTuner.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Wrapping the game layer's own visit() is what makes the whole effect
// tractable. GD offers no clean "wrap the frame" hook -- CCDirector::drawScene
// swaps buffers before a composite would be possible, and CCNode::visit would
// fire for every node in the game -- but GJBaseGameLayer overrides visit() and
// both PlayLayer and LevelEditorLayer inherit it. So one hook captures exactly
// the game, in one function, with the render target's begin/end trivially
// balanced.
class $modify(FRTXGameLayer, GJBaseGameLayer) {
    void visit() {
        auto& post = frtx::PostProcessor::get();
        auto const& cfg = FRTXConfig::current();

        bool const inEditor = typeinfo_cast<LevelEditorLayer*>(this) != nullptr;
        if (inEditor && !cfg.enableInEditor) {
            GJBaseGameLayer::visit();
            return;
        }

        frtx::FrameInfo info;
        if (m_player1 && m_player1->getParent()) {
            auto const world = m_player1->getParent()->convertToWorldSpace(m_player1->getPosition());
            auto const winSize = CCDirector::sharedDirector()->getWinSize();
            if (winSize.width > 0.0f && winSize.height > 0.0f) {
                info.hasPlayer = true;
                info.playerX = world.x / winSize.width;
                info.playerY = world.y / winSize.height;
            }
        }

        if (!post.beginCapture(info)) {
            GJBaseGameLayer::visit();
            if (auto tuner = FRTXTuner::get()) tuner->tick();
            return;
        }

        // Only lift the UI out if it really is our child; on any layout where
        // it lives elsewhere it is already outside the capture and hiding it
        // here would just make it disappear.
        bool const liftUI = cfg.excludeUI && m_uiLayer && m_uiLayer->getParent() == this;
        if (liftUI) m_uiLayer->setVisible(false);

        GJBaseGameLayer::visit();

        post.endCapture();

        if (liftUI) {
            m_uiLayer->setVisible(true);
            // Redraw the UI over the finished image, under this layer's own
            // transform so it lands exactly where it would have.
            kmGLPushMatrix();
            this->transform();
            m_uiLayer->visit();
            kmGLPopMatrix();
        }

        if (auto tuner = FRTXTuner::get()) tuner->tick();
    }
};

class $modify(FRTXPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        // Compile and allocate here rather than on the first frame that
        // actually renders, so the cost lands during the level transition
        // instead of as a hitch once the player is already moving.
        frtx::PostProcessor::get().warmUp();
        return true;
    }

    void onQuit() {
        // Not strictly required -- the targets are reused rather than
        // reallocated per level -- but there is no reason to hold several
        // megabytes of framebuffer while sitting in the menus.
        frtx::PostProcessor::get().releaseResources();
        PlayLayer::onQuit();
    }
};

class $modify(FRTXEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;
        if (FRTXConfig::current().enableInEditor) {
            frtx::PostProcessor::get().warmUp();
        }
        return true;
    }
};

$on_mod(Loaded) {
    // Reading forty-odd settings costs a hash lookup and a dynamic_cast each,
    // which is far too much to repeat every frame. Cache the snapshot and drop
    // it only when something actually changes; Geode sends this synchronously
    // from setValue, so both the settings menu and the tuner are covered.
    listenForAllSettingChanges([](auto, auto) {
        FRTXConfig::invalidate();
    });

    FRTXTuner::registerListeners();
    log::info("FRTX loaded");
}
