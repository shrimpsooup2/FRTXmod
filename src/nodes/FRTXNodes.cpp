#include "FRTXNodes.hpp"

#include "../render/FRTXPostProcessor.hpp"

using namespace geode::prelude;

namespace {
    template <class T>
    T* createBracketNode() {
        auto node = new T();
        if (node->init()) {
            node->setPosition(0.0f, 0.0f);
            node->setAnchorPoint(ccp(0.0f, 0.0f));
            node->setScale(1.0f);
            node->setRotation(0.0f);
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }
}

FRTXCaptureNode* FRTXCaptureNode::create() {
    return createBracketNode<FRTXCaptureNode>();
}

void FRTXCaptureNode::draw() {
    frtx::PostProcessor::get().beginCapture();
}

FRTXCompositeNode* FRTXCompositeNode::create() {
    return createBracketNode<FRTXCompositeNode>();
}

void FRTXCompositeNode::draw() {
    frtx::PostProcessor::get().endCapture();
}
