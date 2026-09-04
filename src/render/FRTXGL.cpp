#include "FRTXGL.hpp"

#include "FRTXShaders.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace frtx {

bool Target::create(int wPoints, int hPoints) {
    destroy();

    wPoints = std::max(wPoints, 4);
    hPoints = std::max(hPoints, 4);

    auto texture = CCRenderTexture::create(wPoints, hPoints);
    if (!texture) {
        log::error("failed to create a {}x{} render texture", wPoints, hPoints);
        return false;
    }

    auto sprite = texture->getSprite();
    if (!sprite || !sprite->getTexture()) {
        log::error("render texture came back without a backing texture");
        return false;
    }

    auto tex = sprite->getTexture();
    // Bilinear filtering is what makes the five-tap gaussian and the upsample
    // in the composite pass behave like a proper blur rather than a mosaic.
    tex->setAntiAliasTexParameters();

    auto const potW = static_cast<float>(tex->getPixelsWide());
    auto const potH = static_cast<float>(tex->getPixelsHigh());
    if (potW <= 0.0f || potH <= 0.0f) {
        log::error("render texture reported a zero size");
        return false;
    }

    // CCRenderTexture scales the requested point size by the content scale
    // factor before allocating, so mirror that to find the used region.
    auto const scale = CC_CONTENT_SCALE_FACTOR();
    auto const contentW = static_cast<float>(static_cast<int>(wPoints * scale));
    auto const contentH = static_cast<float>(static_cast<int>(hPoints * scale));

    // Discover the framebuffer id once, here, where begin()/end() disturbing
    // the projection costs nothing. Every later pass binds it directly.
    GLint previousFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
    texture->begin();
    GLint discovered = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &discovered);
    texture->end();
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFBO));

    if (discovered == 0) {
        log::error("could not determine the framebuffer for a render target");
        return false;
    }

    rt = texture;
    fbo = static_cast<GLuint>(discovered);
    widthPoints = wPoints;
    heightPoints = hPoints;
    pixelWidth = static_cast<int>(contentW);
    pixelHeight = static_cast<int>(contentH);
    uvW = contentW / potW;
    uvH = contentH / potH;
    texelW = 1.0f / potW;
    texelH = 1.0f / potH;

    return true;
}

void Target::destroy() {
    rt = nullptr;
    fbo = 0;
    widthPoints = 0;
    heightPoints = 0;
    pixelWidth = 0;
    pixelHeight = 0;
    uvW = uvH = 1.0f;
    texelW = texelH = 0.0f;
}

GLuint Target::textureName() const {
    if (!rt) return 0;
    auto sprite = rt->getSprite();
    if (!sprite || !sprite->getTexture()) return 0;
    return sprite->getTexture()->getName();
}

bool Program::init(char const* name, char const* fragmentSource) {
    destroy();

    auto program = new CCGLProgram();
    if (!program->initWithVertexShaderByteArray(shaders::VERTEX, fragmentSource)) {
        log::error("shader '{}' failed to compile (see the cocos log above)", name);
        program->release();
        return false;
    }

    program->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
    program->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
    program->link();
    program->updateUniforms();

    m_program = program;
    return true;
}

void Program::destroy() {
    m_locations.clear();
    if (m_program) {
        m_program->release();
        m_program = nullptr;
    }
}

void Program::use() {
    if (m_program) m_program->use();
}

GLint Program::location(char const* name) {
    if (!m_program) return -1;

    auto it = m_locations.find(name);
    if (it != m_locations.end()) return it->second;

    auto loc = m_program->getUniformLocationForName(name);
    m_locations.emplace(name, loc);
    return loc;
}

void Program::set1i(char const* name, int value) {
    auto loc = location(name);
    if (loc >= 0) m_program->setUniformLocationWith1i(loc, value);
}

void Program::set1f(char const* name, float value) {
    auto loc = location(name);
    if (loc >= 0) m_program->setUniformLocationWith1f(loc, value);
}

void Program::set2f(char const* name, float x, float y) {
    auto loc = location(name);
    if (loc >= 0) m_program->setUniformLocationWith2f(loc, x, y);
}

void Program::set3f(char const* name, float x, float y, float z) {
    auto loc = location(name);
    if (loc >= 0) m_program->setUniformLocationWith3f(loc, x, y, z);
}

void Program::set4f(char const* name, float x, float y, float z, float w) {
    auto loc = location(name);
    if (loc >= 0) m_program->setUniformLocationWith4f(loc, x, y, z, w);
}

void bindTexture(int unit, GLuint textureName) {
    ccGLBindTexture2DN(static_cast<GLuint>(unit), textureName);
}

GLViewportState captureGLState() {
    GLViewportState state;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state.fbo);
    glGetIntegerv(GL_VIEWPORT, state.viewport);
    state.scissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetBooleanv(GL_COLOR_WRITEMASK, state.colorMask);
    return state;
}

void restoreGLState(GLViewportState const& state) {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(state.fbo));
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
}

void suspendClipping() {
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void restoreClipping(GLViewportState const& state) {
    if (state.scissor) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    glColorMask(state.colorMask[0], state.colorMask[1],
                state.colorMask[2], state.colorMask[3]);
}

void bindTargetForDrawing(Target const& target) {
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, target.pixelWidth, target.pixelHeight);
}

void drawFullscreenQuad() {
    GLfloat const positions[8] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    // Clip space and texture space share an origin here: NDC (-1, -1) is the
    // bottom-left of the viewport and uv (0, 0) is the bottom-left texel, so a
    // straight mapping keeps the image upright with no flip.
    GLfloat const texCoords[8] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };

#if CC_TEXTURE_ATLAS_USE_VAO
    ccGLBindVAO(0);
#endif
    // A leftover vertex buffer binding would make the pointers below be read as
    // byte offsets into that buffer instead of as client memory.
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position | kCCVertexAttribFlag_TexCoords);
    glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, 0, positions);
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

}
