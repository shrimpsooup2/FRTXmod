#pragma once

#include "nodes/FRTXNodes.hpp"

#include <Geode/Geode.hpp>

// Keeps the capture/composite pair bracketing the gameplay layer.
//
// Attaching is done from the layer's update() rather than once at init(), for
// two reasons: the layer has no parent yet while it is being constructed, and
// re-checking every frame means the pair heals itself if a scene transition or
// another mod reparents things underneath us.
class FRTXManager {
public:
    static FRTXManager& get();

    void attachTo(cocos2d::CCNode* gameLayer);
    void detach();

private:
    geode::Ref<FRTXCaptureNode> m_capture = nullptr;
    geode::Ref<FRTXCompositeNode> m_composite = nullptr;
};
