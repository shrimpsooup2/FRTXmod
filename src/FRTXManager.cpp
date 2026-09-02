#include "FRTXManager.hpp"

#include "render/FRTXPostProcessor.hpp"

#include <limits>

using namespace geode::prelude;

namespace {
    // Nothing can sort outside these, so the pair always brackets every sibling.
    constexpr int kCaptureZOrder = std::numeric_limits<int>::min();
    constexpr int kCompositeZOrder = std::numeric_limits<int>::max();
}

FRTXManager& FRTXManager::get() {
    static FRTXManager instance;
    return instance;
}

void FRTXManager::attachTo(CCNode* gameLayer) {
    if (!gameLayer) return;

    auto scene = gameLayer->getParent();
    if (!scene) return;

    if (!m_capture) m_capture = FRTXCaptureNode::create();
    if (!m_composite) m_composite = FRTXCompositeNode::create();
    if (!m_capture || !m_composite) return;

    if (m_capture->getParent() != scene) {
        m_capture->removeFromParentAndCleanup(true);
        scene->addChild(m_capture, kCaptureZOrder);
    }
    if (m_composite->getParent() != scene) {
        m_composite->removeFromParentAndCleanup(true);
        scene->addChild(m_composite, kCompositeZOrder);
    }
}

void FRTXManager::detach() {
    if (m_capture) m_capture->removeFromParentAndCleanup(true);
    if (m_composite) m_composite->removeFromParentAndCleanup(true);
    frtx::PostProcessor::get().releaseResources();
}
