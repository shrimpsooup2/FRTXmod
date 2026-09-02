#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <unordered_map>

namespace frtx {

// A render target plus the numbers needed to sample it correctly.
//
// cocos pads a render texture up to a power of two when the driver does not
// advertise NPOT support, while only the lower-left corner actually holds the
// image. `uvW`/`uvH` are the usable slice of that texture, and `texelW`/`texelH`
// are one texel in the same uv space.
struct Target {
    geode::Ref<cocos2d::CCRenderTexture> rt = nullptr;

    int   widthPoints  = 0;
    int   heightPoints = 0;
    float uvW    = 1.0f;
    float uvH    = 1.0f;
    float texelW = 0.0f;
    float texelH = 0.0f;

    bool create(int widthPoints, int heightPoints);
    void destroy();

    bool valid() const { return rt != nullptr; }
    GLuint textureName() const;
};

// Thin wrapper over CCGLProgram that caches uniform locations by name.
class Program {
public:
    ~Program() { destroy(); }

    bool init(char const* name, char const* fragmentSource);
    void destroy();

    bool valid() const { return m_program != nullptr; }

    // Binds the program. Must be called before any of the setters below.
    void use();

    void set1i(char const* name, int value);
    void set1f(char const* name, float value);
    void set2f(char const* name, float x, float y);
    void set3f(char const* name, float x, float y, float z);
    void set4f(char const* name, float x, float y, float z, float w);

private:
    GLint location(char const* name);

    cocos2d::CCGLProgram* m_program = nullptr;
    std::unordered_map<std::string, GLint> m_locations;
};

// Binds `textureName` to the given texture unit through cocos' state cache, so
// cocos does not later skip a bind it believes is already active.
void bindTexture(int unit, GLuint textureName);

// Draws a fullscreen quad in clip space with texture coordinates in
// normalised screen space (0..1). Each shader scales those by the uv extent of
// whichever target it is reading.
void drawFullscreenQuad();

}
