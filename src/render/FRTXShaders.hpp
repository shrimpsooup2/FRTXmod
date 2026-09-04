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
uniform vec4 u_filter;  // x = threshold, y = knee, z = 1 / (4 * knee), w = emissive bias
uniform vec4 u_filter2; // x = background suppression, yz = wide sample offset, w = isolation boost

varying vec2 v_texCoord;

void main() {
    vec2 screenUV = v_texCoord;
    vec2 uv = screenUV * u_sourceUV;
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

    // Saturation bias: GD's neon, glow objects and particles are strongly
    // saturated, while skies and background gradients are bright but washed
    // out. Pure whites are exempt, because plenty of GD glow is white.
    float saturation = (brightness - darkest) / max(brightness, 0.0001);
    float emissive = max(saturation, smoothstep(0.85, 1.0, brightness));
    contribution *= mix(1.0, emissive, u_filter.w);

    // How bright this pixel is relative to a wide neighbourhood around it.
    // Saturation alone cannot separate a glowing object from a bright saturated
    // backdrop, so ask whether the surroundings are bright too. The
    // neighbourhood is deliberately screen-scale: at a small radius the middle
    // of a large glowing object also looks surrounded by brightness and its
    // glow would come out hollow.
    if (u_filter2.x > 0.0 || u_filter2.w > 0.0) {
        vec2 w = u_filter2.yz;
        vec3 wide = texture2D(u_source, clamp(screenUV + vec2( w.x,  w.y), 0.0, 1.0) * u_sourceUV).rgb;
        wide += texture2D(u_source, clamp(screenUV + vec2(-w.x,  w.y), 0.0, 1.0) * u_sourceUV).rgb;
        wide += texture2D(u_source, clamp(screenUV + vec2( w.x, -w.y), 0.0, 1.0) * u_sourceUV).rgb;
        wide += texture2D(u_source, clamp(screenUV + vec2(-w.x, -w.y), 0.0, 1.0) * u_sourceUV).rgb;
        wide *= 0.25;

        float around = max(wide.r, max(wide.g, wide.b));
        float openness = clamp(1.0 - around / max(brightness, 0.0001), 0.0, 1.0);

        // Suppression holds back a pixel inside a broad bright wash.
        contribution *= mix(1.0, openness, u_filter2.x);
        // Isolation does the opposite: a bright thing alone in the dark glows
        // harder than the same brightness would in a bright area. Rays are
        // built from this buffer, so isolated objects throw more of them too.
        contribution *= 1.0 + u_filter2.w * openness;
    }

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
uniform vec4 u_ray2;     // x = jitter, y = sun intensity, z = sun size, w = shimmer
uniform vec2 u_rayMisc;  // x = aspect, y = time
uniform float u_samples;

varying vec2 v_texCoord;

float rayHash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = v_texCoord;
    vec2 toOrigin = uv - u_origin;

    float weight = u_params.z;
    if (u_ray2.w > 0.0) {
        // Vary strength with the angle around the source, slowly, so the fan of
        // shafts breathes instead of sitting perfectly still.
        float angle = atan(toOrigin.y, toOrigin.x);
        weight *= 1.0 + u_ray2.w * 0.5 * sin(angle * 11.0 + u_rayMisc.y * 1.3);
    }

    vec2 delta = toOrigin * (u_params.x / max(u_samples, 1.0));

    // Start each pixel a random fraction of one step along the ray. Marching
    // from the same distances for every pixel is what produces the concentric
    // stepping that gives cheap god rays away; jittering turns that banding
    // into fine noise, which reads as grain rather than as an artefact.
    float jitter = rayHash(uv * 543.21 + fract(u_rayMisc.y)) * u_ray2.x;
    vec2 coord = uv - delta * jitter;

    vec3 accum = vec3(0.0);
    float illumination = 1.0;

    // The bound is a compile time constant because GLSL ES 1.00 requires it;
    // the break is what actually honours the sample count.
    for (int i = 0; i < 48; ++i) {
        if (float(i) >= u_samples) break;
        // Clamping in screen space keeps the march from wandering into the
        // padding of a power-of-two padded target.
        vec2 tap = clamp(coord, 0.0, 1.0);
        accum += texture2D(u_source, tap * u_sourceUV).rgb * illumination * weight;
        illumination *= u_params.y;
        coord -= delta;
    }

    accum *= u_params.w;

    // A visible source, so the shafts have something to come from. It is gated
    // on light actually being present at the origin: a disc drawn
    // unconditionally sits at a fixed point on screen while the level scrolls
    // past behind it, which reads as a smudge on the lens rather than as a sun.
    if (u_ray2.y > 0.0) {
        vec2 o = u_origin;
        float probe = max(u_ray2.z * 0.5, 0.01);
        vec3 atSource = texture2D(u_source, clamp(o + vec2( probe, 0.0), 0.0, 1.0) * u_sourceUV).rgb;
        atSource += texture2D(u_source, clamp(o + vec2(-probe, 0.0), 0.0, 1.0) * u_sourceUV).rgb;
        atSource += texture2D(u_source, clamp(o + vec2(0.0,  probe), 0.0, 1.0) * u_sourceUV).rgb;
        atSource += texture2D(u_source, clamp(o + vec2(0.0, -probe), 0.0, 1.0) * u_sourceUV).rgb;
        atSource *= 0.25;

        float present = clamp(max(atSource.r, max(atSource.g, atSource.b)) * 3.0, 0.0, 1.0);
        if (present > 0.0) {
            vec2 d = (v_texCoord - o) * vec2(u_rayMisc.x, 1.0);
            float size = max(u_ray2.z, 0.001);
            float disc = exp(-dot(d, d) / (size * size));
            accum += vec3(disc * u_ray2.y * present);
        }
    }

    gl_FragColor = vec4(accum, 1.0);
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
uniform sampler2D u_halationTex;

uniform vec2 u_sceneUV;
uniform vec2 u_bloomUV0;
uniform vec2 u_bloomUV1;
uniform vec2 u_bloomUV2;
uniform vec2 u_streakUV;
uniform vec2 u_raysUV;
uniform vec2 u_halationUV;

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
uniform vec4 u_flare;     // x = ghost intensity, y = spacing, z = halation, w = ray occlusion
uniform vec2 u_lens2;     // x = barrel distortion, y = unused
uniform vec4 u_clarity;   // x = amount, y = radius in screen uv, z = detail clamp, w = use 8 taps

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
    vec2 rd = r * 0.7071;

    // Four diagonal taps estimate the local average well enough at the radii
    // clarity is actually used at, for half the bandwidth of the full ring.
    // These are full resolution fetches, so this is the most expensive thing
    // in the pass and the first place to buy frames back.
    vec3 low = texture2D(u_scene, (screenUV + rd) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV - rd) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV + vec2(rd.x, -rd.y)) * u_sceneUV).rgb;
    low += texture2D(u_scene, (screenUV - vec2(rd.x, -rd.y)) * u_sceneUV).rgb;

    if (u_clarity.w > 0.5) {
        vec2 rx = vec2(r.x, 0.0);
        vec2 ry = vec2(0.0, r.y);
        low += texture2D(u_scene, (screenUV + rx) * u_sceneUV).rgb;
        low += texture2D(u_scene, (screenUV - rx) * u_sceneUV).rgb;
        low += texture2D(u_scene, (screenUV + ry) * u_sceneUV).rgb;
        low += texture2D(u_scene, (screenUV - ry) * u_sceneUV).rgb;
        low *= 0.125;
    } else {
        low *= 0.25;
    }

    vec3 detail = color - low;
    detail = sign(detail) * min(abs(detail), vec3(u_clarity.z));
    return color + detail * u_clarity.x;
}

void main() {
    vec2 uv = v_texCoord;

    // Lens distortion has to come first: everything downstream samples through
    // this coordinate, so bending it here bends the whole image rather than
    // just one layer of it.
    if (u_lens2.x != 0.0) {
        vec2 centred = uv - vec2(0.5);
        uv = vec2(0.5) + centred * (1.0 + u_lens2.x * dot(centred, centred));
        uv = clamp(uv, 0.0, 1.0);
    }

    vec3 color = sampleScene(uv, u_lens.x);
    if (u_clarity.x > 0.0) {
        color = applyClarity(color, uv);
    }

    // Each of these is a full resolution fetch, and an effect that is switched
    // off still has a texture bound to its unit. Branching on the weight is
    // uniform across the whole draw, so it costs nothing when taken and saves
    // real bandwidth in every configuration that does not use all of them --
    // which is the default one.
    vec3 bloom = vec3(0.0);
    if (u_bloom.x > 0.0) bloom += texture2D(u_bloom0, uv * u_bloomUV0).rgb * u_bloom.x;
    if (u_bloom.y > 0.0) bloom += texture2D(u_bloom1, uv * u_bloomUV1).rgb * u_bloom.y;
    if (u_bloom.z > 0.0) bloom += texture2D(u_bloom2, uv * u_bloomUV2).rgb * u_bloom.z;
    bloom *= u_bloom.w * u_bloomTint;

    vec3 streak = vec3(0.0);
    if (dot(u_streak, vec3(1.0)) > 0.0) {
        streak = texture2D(u_streakTex, uv * u_streakUV).rgb * u_streak;
    }

    vec3 rays = vec3(0.0);
    if (dot(u_rays, vec3(1.0)) > 0.0) {
        rays = texture2D(u_raysTex, uv * u_raysUV).rgb * u_rays;
        // Light shafts are light in the air between the camera and the source,
        // so something solid and dark in front of them should block them.
        // Without this they wash over the player and read as a flat overlay.
        if (u_flare.w > 0.0) {
            float sceneLuma = dot(color, LUMA);
            rays *= mix(1.0, smoothstep(0.0, 0.35, sceneLuma), u_flare.w);
        }
    }

    // Ghost images of bright areas, mirrored through the centre of the screen
    // the way light bounces between the elements of a real lens.
    vec3 ghosts = vec3(0.0);
    if (u_flare.x > 0.0 && u_bloom.x > 0.0) {
        vec2 toCentre = vec2(0.5) - uv;
        for (int g = 1; g <= 4; ++g) {
            vec2 sampleUV = uv + toCentre * (float(g) * u_flare.y);
            float falloff = 1.0 - clamp(length(sampleUV - vec2(0.5)) * 1.7, 0.0, 1.0);
            ghosts += texture2D(u_bloom0, clamp(sampleUV, 0.0, 1.0) * u_bloomUV0).rgb
                    * falloff * falloff;
        }
        ghosts *= u_flare.x;
    }

    // Halation is not more bloom: film scatters light back through its own base
    // and it returns wide, soft and strongly red. Driving it from the widest
    // bloom level and tinting it hard is what separates the two.
    vec3 halation = vec3(0.0);
    if (u_flare.z > 0.0) {
        halation = texture2D(u_halationTex, uv * u_halationUV).rgb
                 * u_flare.z * vec3(1.0, 0.32, 0.18);
    }

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
    if (u_misc.z > 3.5 && u_misc.z < 4.5) {
        gl_FragColor = vec4(rays, 1.0);
        return;
    }
    if (u_misc.z > 4.5) {
        gl_FragColor = vec4(ghosts + halation, 1.0);
        return;
    }

    color += bloom + streak + rays + ghosts + halation;

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
