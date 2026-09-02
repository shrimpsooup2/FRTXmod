#pragma once

#include <Geode/Geode.hpp>

// The two halves of the frame bracket.
//
// Both are added as siblings of the gameplay layer inside its scene, one with
// the lowest possible z-order and one with the highest. cocos visits children in
// z-order, so everything drawn between the two ends up in our capture target.
//
// Neither node may carry a transform: CCRenderTexture pushes matrices in
// begin() and pops them in end(), and those calls land in two different visit()
// bodies. The push/pop counts still balance, but only an identity transform
// keeps the modelview the scene renders under unchanged.

class FRTXCaptureNode : public cocos2d::CCNode {
public:
    static FRTXCaptureNode* create();
    void draw() override;
};

class FRTXCompositeNode : public cocos2d::CCNode {
public:
    static FRTXCompositeNode* create();
    void draw() override;
};
