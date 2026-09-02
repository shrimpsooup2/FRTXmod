#pragma once

// All GLSL used by the mod.
//
// These are written for GLSL ES 1.00 / desktop GLSL 1.20 with *no* version
// directive, because cocos2d-x prepends its own preamble (precision qualifiers
// on GLES, plus the CC_* built-in uniforms) before compiling. Sticking to
// `attribute` / `varying` / `texture2D` keeps the same source valid on both
// desktop OpenGL and OpenGL ES 2.0.
//
// Every pass receives `v_texCoord` in normalised *screen* space (0..1) rather
// than in texture space. Each shader then scales it by the `u_*UV` extent of the
// texture it is about to read. That indirection exists because a cocos render
// texture is padded up to a power of two when the driver lacks NPOT support, so
// the image only occupies part of the allocated texture, and the fraction is
// different for every buffer in the chain.

namespace frtx::shaders {

// Positions arrive already in clip space, so the vertex shader deliberately
// ignores CC_MVPMatrix. That makes the passes immune to whatever projection
// cocos happens to have set up when a render texture is bound.
inline constexpr char const* VERTEX = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;

varying vec2 v_texCoord;

void main() {
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";

// Bright pass: box-downsample the scene and keep only what is above the
// threshold, with a soft knee so highlights fade in instead of popping.
//
// The contribution is also weighted towards saturated pixels. GD's neon, glow
// objects and particles are strongly saturated, while skies and background
// gradients are bright but washed out, so this is what keeps a bright
// background from blooming as hard as the objects in front of it. Pure whites
// are exempted, because plenty of GD glow is white.
inline constexpr char const* PREFILTER = R"(
uniform sampler2D u_source;
uniform vec2 u_sourceUV;
uniform vec2 u_texelSize;
uniform vec4 u_filter; // x = threshold, y = knee, z = 1 / (4 * knee), w = emissive bias

varying vec2 v_texCoord;

void main() {
    vec2 uv = v_texCoord * u_sourceUV;
    vec2 o = u_texelSize;

    vec3 c = texture2D(u_source, uv + vec2(-o.x, -o.y)).rgb;
    c += texture2D(u_source, uv + vec2( o.x, -o.y)).rgb;
    c += texture2D(u_source, uv + vec2(-o.x,  o.y)).rgb;
    c += texture2D(u_source, uv + vec2( o.x,  o.y)).rgb;
    c *= 0.25;

    float brightness = max(c.r, max(c.g, c.b));
    float darkest = min(c.r, min(c.g, c.b));

    float soft = brightness - u_filter.x + u_filter.y;
    soft = clamp(soft, 0.0, 2.0 * u_filter.y);
    soft = soft * soft * u_filter.z;

    float contribution = max(soft, brightness - u_filter.x) / max(brightness, 0.0001);

    float saturation = (brightness - darkest) / max(brightness, 0.0001);
    float emissive = max(saturation, smoothstep(0.85, 1.0, brightness));
    contribution *= mix(1.0, emissive, u_filter.w);

    gl_FragColor = vec4(c * contribution, 1.0);
}
)";

// Plain 4-tap box downsample used to build the lower bloom levels.
inline constexpr char const* DOWNSAMPLE = R"(
uniform sampler2D u_source;
uniform vec2 u_sourceUV;
uniform vec2 u_texelSize;

varying vec2 v_texCoord;

void main() {
    vec2 uv = v_texCoord * u_sourceUV;
    vec2 o = u_texelSize;

    vec3 c = texture2D(u_source, uv + vec2(-o.x, -o.y)).rgb;
    c += texture2D(u_source, uv + vec2( o.x, -o.y)).rgb;
    c += texture2D(u_source, uv + vec2(-o.x,  o.y)).rgb;
    c += texture2D(u_source, uv + vec2( o.x,  o.y)).rgb;

    gl_FragColor = vec4(c * 0.25, 1.0);
}
)";

// Separable gaussian. Nine taps folded into five by leaning on bilinear
// filtering to fetch two texels at a time.
inline constexpr char const* BLUR = R"(
uniform sampler2D u_source;
uniform vec2 u_sourceUV;
uniform vec2 u_offset; // direction * texel size * radius

varying vec2 v_texCoord;

void main() {
    vec2 uv = v_texCoord * u_sourceUV;
    vec2 o1 = u_offset * 1.3846153846;
    vec2 o2 = u_offset * 3.2307692308;

    vec3 c = texture2D(u_source, uv).rgb * 0.2270270270;
    c += (texture2D(u_source, uv + o1).rgb +
          texture2D(u_source, uv - o1).rgb) * 0.3162162162;
    c += (texture2D(u_source, uv + o2).rgb +
          texture2D(u_source, uv - o2).rgb) * 0.0702702703;

    gl_FragColor = vec4(c, 1.0);
}
)";

// Radial light shafts, the Kenny Mitchell formulation: march from the pixel
// back towards the light origin along a straight line, accumulating the bright
// pass and attenuating as we go. Run on the bright pass rather than the scene
// so only things that already glow can cast a shaft.
inline constexpr char const* RAYS = R"(
uniform sampler2D u_source;
uniform vec2 u_sourceUV;
uniform vec2 u_origin;   // light position in normalised screen space
uniform vec4 u_params;   // x = density, y = decay, z = weight, w = normalisation
uniform float u_samples;

varying vec2 v_texCoord;

void main() {
    vec2 uv = v_texCoord;
    vec2 delta = (uv - u_origin) * (u_params.x / max(u_samples, 1.0));

    vec3 accum = texture2D(u_source, uv * u_sourceUV).rgb;
    vec2 coord = uv;
    float illumination = 1.0;

    // The bound is a compile time constant because GLSL ES 1.00 requires it;
    // the break is what actually honours the sample count.
    for (int i = 0; i < 48; ++i) {
        if (float(i) >= u_samples) break;
        coord -= delta;
        // Clamping in screen space keeps the march from wandering into the
        // padding of a power-of-two padded target.
        vec2 tap = clamp(coord, 0.0, 1.0) * u_sourceUV;
        accum += texture2D(u_source, tap).rgb * illumination * u_params.z;
        illumination *= u_params.y;
    }

    gl_FragColor = vec4(accum * u_params.w, 1.0);
}
)";

// Everything that touches the final image happens here in one pass: clarity,
// bloom and streak combine, tonemap, grade and lens effects.
inline constexpr char const* COMPOSITE = R"(
uniform sampler2D u_scene;
uniform sampler2D u_bloom0;
uniform sampler2D u_bloom1;
uniform sampler2D u_bloom2;
uniform sampler2D u_streakTex;
uniform sampler2D u_raysTex;

uniform vec2 u_sceneUV;
uniform vec2 u_bloomUV0;
uniform vec2 u_bloomUV1;
uniform vec2 u_bloomUV2;
uniform vec2 u_streakUV;
uniform vec2 u_raysUV;

uniform vec4 u_bloom;     // xyz = per level weights, w = intensity
uniform vec3 u_bloomTint;
uniform vec3 u_streak;    // tint already multiplied by intensity
uniform vec3 u_rays;      // tint already multiplied by intensity
uniform vec4 u_tone;      // x = exposure, y = contrast, z = saturation, w = tonemap on
uniform vec4 u_lens;      // x = chromatic, y = grain, z = dither on, w = unused
uniform vec4 u_vignette;  // x = strength, y = roundness, z = inner, w = outer
uniform vec4 u_misc;      // x = aspect, y = time, z = debug view, w = unused
uniform vec2 u_grade;     // x = temperature, y = tint
uniform vec4 u_grade2;    // x = black point, y = shadow split, z = highlight split
uniform vec3 u_clarity;   // x = amount, y = radius in screen uv, z = detail clamp

varying vec2 v_texCoord;

const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

// Narkowicz's ACES filmic curve fit.
vec3 tonemapACES(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 sampleScene(vec2 screenUV, float amount) {
    if (amount <= 0.0) {
        return texture2D(u_scene, screenUV * u_sceneUV).rgb;
    }
    // The offset grows with the square of the distance from the centre, so the
    // middle of the screen stays perfectly sharp.
    vec2 d = screenUV - vec2(0.5);
    vec2 offset = d * dot(d, d) * amount * 0.15;
    return vec3(
        texture2D(u_scene, (screenUV - offset) * u_sceneUV).r,
        texture2D(u_scene, screenUV * u_sceneUV).g,
        texture2D(u_scene, (screenUV + offset) * u_sceneUV).b
    );
}

// Unsharp mask against a ring of wide taps. Adding back the difference between
// the image and its own blur lifts edge and material detail, which is what
// makes a flat 2D frame read as though it has depth.
//
// The difference is clamped before it is added back. Without that, the dark
// side of a bright edge gets pushed darker still and every glowing outline
// picks up a dark ring around it, which is exactly the artefact that gives a
// sharpening filter away.
vec3 applyClarity(vec3 color, vec2 screenUV) {
    // The radius arrives normalised against screen height, so the horizontal
    // component is divided by the aspect ratio to keep the ring circular.
    vec2 r = vec2(u_clarity.y / u_misc.x, u_clarity.y);
    vec2 rx = vec2(r.x, 0.0);
    vec2 ry = vec2(0.0, r.y);
    vec2 rd = r * 0.7071;

    vec3 low = texture2D(u_scene, (screenUV + rx) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV - rx) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV + ry) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV - ry) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV + rd) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV - rd) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV + vec2(rd.x, -rd.y)) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV - vec2(rd.x, -rd.y)) * u_sceneUV).rgb;
    low *= 0.125;

    vec3 detail = color - low;
    detail = sign(detail) * min(abs(detail), vec3(u_clarity.z));
    return color + detail * u_clarity.x;
}

void main() {
    vec2 uv = v_texCoord;

    vec3 color = sampleScene(uv, u_lens.x);
    if (u_clarity.x > 0.0) {
        color = applyClarity(color, uv);
    }

    vec3 bloom = texture2D(u_bloom0, uv * u_bloomUV0).rgb * u_bloom.x
               + texture2D(u_bloom1, uv * u_bloomUV1).rgb * u_bloom.y
               + texture2D(u_bloom2, uv * u_bloomUV2).rgb * u_bloom.z;
    bloom *= u_bloom.w * u_bloomTint;

    vec3 streak = texture2D(u_streakTex, uv * u_streakUV).rgb * u_streak;
    vec3 rays = texture2D(u_raysTex, uv * u_raysUV).rgb * u_rays;

    if (u_misc.z > 0.5 && u_misc.z < 1.5) {
        gl_FragColor = vec4(color, 1.0);
        return;
    }
    if (u_misc.z > 1.5 && u_misc.z < 2.5) {
        gl_FragColor = vec4(bloom, 1.0);
        return;
    }
    if (u_misc.z > 2.5 && u_misc.z < 3.5) {
        gl_FragColor = vec4(streak, 1.0);
        return;
    }
    if (u_misc.z > 3.5) {
        gl_FragColor = vec4(rays, 1.0);
        return;
    }

    color += bloom + streak + rays;

    color *= u_tone.x;
    if (u_tone.w > 0.5) {
        color = tonemapACES(color);
    } else {
        color = clamp(color, 0.0, 1.0);
    }

    // Crushing the black point is most of what separates "a filter" from
    // "footage": it is applied after the tonemap so the curve has already
    // decided how the highlights roll off.
    color = max(color - vec3(u_grade2.x), vec3(0.0)) / max(1.0 - u_grade2.x, 0.001);

    // White balance. Both curves are the identity at 0 so the neutral setting
    // leaves the image untouched.
    color *= vec3(1.0) + vec3( 0.06, -0.02, -0.10) * u_grade.x;
    color *= vec3(1.0) + vec3(-0.04,  0.05, -0.04) * u_grade.y;

    // Split toning: cool the shadows, warm the highlights, leave the midtones.
    float luma = dot(color, LUMA);
    vec3 shadowTint = vec3(1.0) + vec3(-0.08, -0.02, 0.10) * u_grade2.y;
    vec3 highlightTint = vec3(1.0) + vec3(0.10, 0.03, -0.08) * u_grade2.z;
    color *= mix(shadowTint, vec3(1.0), smoothstep(0.0, 0.5, luma));
    color *= mix(vec3(1.0), highlightTint, smoothstep(0.4, 1.0, luma));

    color = (color - 0.5) * u_tone.y + 0.5;
    color = mix(vec3(dot(color, LUMA)), color, u_tone.z);

    if (u_vignette.x > 0.0) {
        // Roundness blends between following the shape of the screen and a
        // true circle.
        vec2 d = (uv - vec2(0.5)) * vec2(mix(1.0, u_misc.x, u_vignette.y), 1.0);
        float v = 1.0 - smoothstep(u_vignette.z, u_vignette.w, length(d));
        color *= mix(1.0, v, u_vignette.x);
    }

    float noise = fract(sin(dot(uv + fract(u_misc.y), vec2(12.9898, 78.233))) * 43758.5453);
    if (u_lens.y > 0.0) {
        color += (noise - 0.5) * u_lens.y;
    }
    if (u_lens.z > 0.5) {
        // Roughly half a least significant bit, enough to break up banding.
        color += (noise - 0.5) * (1.0 / 255.0);
    }

    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)";

}
