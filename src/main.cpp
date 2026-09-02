#include "FRTXManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(FRTXPlayLayer, PlayLayer) {
    // The scheduler runs before the running scene is visited, so re-checking
    // here takes effect in the very same frame.
    void update(float dt) {
        PlayLayer::update(dt);
        FRTXManager::get().attachTo(this);
    }

    void onQuit() {
        FRTXManager::get().detach();
        PlayLayer::onQuit();
    }
};

$on_mod(Loaded) {
    log::info("FRTX loaded");
}
