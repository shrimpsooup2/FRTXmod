#!/usr/bin/env python3
"""Single source of truth for FRTX's settings.

Generates both `mod.json`'s settings block and `src/FRTXParams.inc`, the
X-macro list that `FRTXConfig::read()` and the in-game tuner are both built
from. With forty-odd controls, keeping the JSON schema, the config reader and
the tuner's table in sync by hand is a losing game; this makes them one edit.

Run from the repo root after changing anything below:

    python3 tools/gen_settings.py
"""

import collections
import json
import pathlib

# key, member, label, min, max, default, step
FLOAT = 'float'
INT = 'int'
BOOL = 'bool'
COLOR = 'color'
TITLE = 'title'

# (kind, key, member, label, description, min, max, default, step)
SPEC = [
    (TITLE, None, None, "General", None, None, None, None, None),
    (BOOL, "enabled", "enabled", "Enabled",
     "Master switch. When off the game renders completely untouched and no GPU memory is used.",
     None, None, True, None),
    (INT, "preset", "preset", "Preset",
     "0 = Custom, 1 = Subtle, 2 = Showcase, 3 = Overkill, 4 = Performance. Anything other than Custom overrides every look setting below, so set this to 0 before tuning by hand.",
     0, 4, 2, 1),
    (BOOL, "enable-in-editor", "enableInEditor", "In Editor",
     "Also apply the effect inside the level editor.", None, None, False, None),
    (BOOL, "exclude-ui", "excludeUI", "Exclude UI",
     "Draw the gameplay UI on top of the effect instead of through it, so the attempt counter and progress bar stay crisp.",
     None, None, False, None),
    (INT, "debug-view", "debugView", "Debug View",
     "0 = normal, 1 = captured scene, 2 = bloom, 3 = streaks, 4 = light rays. Invaluable for setting the bloom threshold.",
     0, 4, 0, 1),

    (TITLE, None, None, "Bloom", None, None, None, None, None),
    (BOOL, "bloom-enabled", "bloomEnabled", "Bloom",
     "Makes bright objects, glow and particles bleed light into their surroundings.",
     None, None, True, None),
    (FLOAT, "bloom-intensity", "bloomIntensity", "Intensity",
     "How strongly the blurred highlights are added back on top of the scene.", 0.0, 4.0, 1.6, 0.05),
    (FLOAT, "bloom-threshold", "bloomThreshold", "Threshold",
     "Brightness a pixel must reach before it starts to glow. Lower values make more of the screen bloom.",
     0.0, 1.5, 0.52, 0.01),
    (FLOAT, "bloom-knee", "bloomKnee", "Soft Knee",
     "Softness of the threshold. 0 is a hard cutoff, higher values fade highlights in gradually.",
     0.0, 1.0, 0.45, 0.01),
    (FLOAT, "bloom-emissive-bias", "emissiveBias", "Emissive Bias",
     "Biases the glow towards saturated colours, so neon and glow objects bloom hard while bright backgrounds stay put. Pure white still glows.",
     0.0, 1.0, 0.55, 0.01),
    (FLOAT, "bloom-radius", "bloomRadius", "Radius",
     "Scales the blur step size. Larger values spread the glow further at the cost of some fidelity.",
     0.25, 4.0, 2.0, 0.05),
    (FLOAT, "bloom-spread", "bloomSpread", "Spread",
     "How large the halo reads. Low values keep the glow tight against the object; high values let the widest blur levels through.",
     0.0, 1.0, 0.8, 0.01),
    (INT, "bloom-levels", "bloomLevels", "Levels",
     "Number of blur levels. 1 is a tight glow and cheapest, 3 gives a wide cinematic falloff.", 1, 3, 3, 1),
    (FLOAT, "bloom-scale", "bloomScale", "Resolution Scale",
     "Resolution of the bloom buffers relative to the screen. Lower is faster and slightly softer.",
     0.25, 1.0, 0.5, 0.05),
    (COLOR, "bloom-tint", "bloomTint", "Tint",
     "Colours the glow. White leaves it the colour of whatever is glowing.",
     None, None, (255, 255, 255), None),

    (TITLE, None, None, "Anamorphic Streaks", None, None, None, None, None),
    (FLOAT, "streak-intensity", "streakIntensity", "Intensity",
     "Horizontal light streaks off bright points, like a cinema lens. Off by default: showcase levels glow radially rather than streaking. Needs bloom enabled.",
     0.0, 2.0, 0.0, 0.02),
    (FLOAT, "streak-length", "streakLength", "Length",
     "How far the streaks reach across the screen.", 0.25, 4.0, 1.4, 0.05),
    (COLOR, "streak-tint", "streakTint", "Tint",
     "A faint cool tint is what reads as an anamorphic lens rather than as more bloom.",
     None, None, (204, 224, 255), None),

    (TITLE, None, None, "Light Rays", None, None, None, None, None),
    (FLOAT, "rays-intensity", "raysIntensity", "Intensity",
     "Volumetric-looking shafts of light radiating from a point. Needs bloom enabled. 0 disables the pass entirely.",
     0.0, 2.0, 0.0, 0.02),
    (FLOAT, "rays-density", "raysDensity", "Density",
     "How far along the ray direction the samples are spread. Higher values give longer shafts.",
     0.0, 2.0, 0.85, 0.01),
    (FLOAT, "rays-decay", "raysDecay", "Decay",
     "How quickly a ray fades as it travels. Just below 1 gives long soft shafts; lower values cut them short.",
     0.5, 1.0, 0.95, 0.005),
    (FLOAT, "rays-weight", "raysWeight", "Weight",
     "Contribution of each sample along the ray.", 0.0, 1.0, 0.35, 0.01),
    (INT, "rays-samples", "raysSamples", "Samples",
     "Samples per pixel. The single biggest cost in this pass; lower it first if rays are slow.",
     8, 48, 24, 2),
    (INT, "rays-origin-mode", "raysOriginMode", "Origin",
     "0 = a fixed point on screen, 1 = follow the player.", 0, 1, 0, 1),
    (FLOAT, "rays-origin-x", "raysOriginX", "Origin X",
     "Horizontal position of the light source, 0 is the left edge and 1 the right. Used when Origin is 0.",
     0.0, 1.0, 0.5, 0.01),
    (FLOAT, "rays-origin-y", "raysOriginY", "Origin Y",
     "Vertical position of the light source, 0 is the bottom edge and 1 the top. Used when Origin is 0.",
     0.0, 1.0, 0.78, 0.01),
    (COLOR, "rays-tint", "raysTint", "Tint",
     "Colours the shafts. A warm tint reads as sunlight.", None, None, (255, 238, 204), None),

    (TITLE, None, None, "Clarity", None, None, None, None, None),
    (FLOAT, "clarity", "clarity", "Amount",
     "Local contrast. Lifts edge and material detail so the frame reads as though it has depth. Pushed hard it rings bright outlines.",
     0.0, 1.5, 0.12, 0.01),
    (FLOAT, "clarity-radius", "clarityRadius", "Radius",
     "Radius in pixels. Small values sharpen, larger values give broad depth instead.", 1.0, 16.0, 3.5, 0.25),
    (INT, "clarity-taps", "clarityTaps", "Taps",
     "Samples used to estimate the blur clarity works against. 4 is half the cost and near indistinguishable at small radii; 8 is smoother at large ones. This is the most expensive part of the final pass.",
     4, 8, 4, 4),

    (TITLE, None, None, "Grade", None, None, None, None, None),
    (BOOL, "tonemap-enabled", "tonemapEnabled", "Filmic Tonemapping",
     "Applies an ACES filmic curve so bright areas roll off smoothly instead of clipping to white.",
     None, None, True, None),
    (FLOAT, "exposure", "exposure", "Exposure",
     "Overall brightness multiplier applied before tonemapping.", 0.25, 2.5, 1.05, 0.01),
    (FLOAT, "black-point", "blackPoint", "Black Point",
     "Crushes the darkest values to true black. A little of this is most of what separates graded footage from a filter.",
     0.0, 0.2, 0.05, 0.005),
    (FLOAT, "contrast", "contrast", "Contrast", "1.0 leaves contrast untouched.", 0.5, 2.0, 1.10, 0.01),
    (FLOAT, "saturation", "saturation", "Saturation", "0 is greyscale, 1.0 is untouched.", 0.0, 2.0, 1.25, 0.01),
    (FLOAT, "split-shadow", "splitShadow", "Shadow Tone",
     "Tints the shadows only. Positive values cool them towards blue, the classic cinematic split.", -1.0, 1.0, 0.20, 0.01),
    (FLOAT, "split-highlight", "splitHighlight", "Highlight Tone",
     "Tints the highlights only. Positive values warm them towards orange.", -1.0, 1.0, 0.0, 0.01),
    (FLOAT, "temperature", "temperature", "Temperature",
     "Whole-image white balance. Negative cools towards blue, positive warms towards orange.", -1.0, 1.0, 0.0, 0.01),
    (FLOAT, "tint", "tint", "Tint",
     "Negative values push towards magenta, positive towards green.", -1.0, 1.0, 0.0, 0.01),

    (TITLE, None, None, "Lens", None, None, None, None, None),
    (FLOAT, "vignette", "vignette", "Vignette", "Darkens the corners of the screen. 0 disables it.", 0.0, 1.0, 0.10, 0.01),
    (FLOAT, "vignette-roundness", "vignetteRoundness", "Vignette Roundness",
     "0 follows the shape of the screen, 1 is a perfect circle.", 0.0, 1.0, 0.5, 0.01),
    (FLOAT, "vignette-softness", "vignetteSoftness", "Vignette Softness",
     "How gradually the darkening comes in from the corners.", 0.0, 1.0, 0.5, 0.01),
    (FLOAT, "chromatic", "chromatic", "Chromatic Aberration",
     "Splits red and blue towards the edges of the screen, like a real lens. 0 disables it.", 0.0, 1.0, 0.06, 0.01),
    (FLOAT, "grain", "grain", "Film Grain", "Adds animated noise. Keep this very low; 0 disables it.", 0.0, 0.15, 0.0, 0.002),
    (BOOL, "dither", "dither", "Dithering",
     "Adds an imperceptible amount of noise to hide colour banding in smooth gradients.", None, None, True, None),
]


def f(v):
    """Emit a float literal C++ will accept.

    %g helpfully strips the decimal point, which turns 0.0 into the invalid
    literal `0f`, so build the text from repr and make sure a point survives.
    """
    text = repr(float(v))
    if 'e' in text or 'E' in text:
        text = f"{float(v):.10f}".rstrip('0')
    if '.' not in text:
        text += '.0'
    return text + 'f'


def gen_mod_json():
    mod = json.load(open('mod.json'), object_pairs_hook=collections.OrderedDict)
    settings = collections.OrderedDict()
    titles = 0
    for kind, key, member, label, desc, lo, hi, default, step in SPEC:
        if kind == TITLE:
            titles += 1
            settings[f"title-{titles}"] = collections.OrderedDict(
                [("name", label), ("type", "title")])
            continue
        s = collections.OrderedDict([("name", label), ("description", desc)])
        if kind == BOOL:
            s["type"] = "bool"
            s["default"] = default
        elif kind == COLOR:
            s["type"] = "rgb"
            s["default"] = collections.OrderedDict(
                [("r", default[0]), ("g", default[1]), ("b", default[2])])
        else:
            s["type"] = kind
            s["default"] = default
            s["min"] = lo
            s["max"] = hi
            s["control"] = collections.OrderedDict([("slider", True), ("input", True)])
        settings[key] = s

    # The tuner's toggle key is a keybind, which does not fit the table above.
    settings["tuner-key"] = collections.OrderedDict([
        ("name", "Open Live Tuner"),
        ("description", "Opens an in-game overlay for adjusting every value below while you play. "
                        "Arrow keys move and adjust, Shift is coarse, Alt is fine, R resets a value."),
        ("type", "keybind"),
        ("category", "universal"),
        ("default", "F8"),
    ])

    mod['settings'] = settings
    pathlib.Path('mod.json').write_text(json.dumps(mod, indent=4) + "\n")
    return len(settings), titles


def gen_inc():
    out = [
        "// GENERATED by tools/gen_settings.py -- do not edit by hand.",
        "//",
        "// X-macro list of every tunable setting. FRTXConfig::read() and the in-game",
        "// tuner are both built from this, so neither can drift from mod.json.",
        "//",
        "//   FRTX_FLOAT(key, member, label, min, max, default, step)",
        "//   FRTX_INT  (key, member, label, min, max, default, step)",
        "//   FRTX_BOOL (key, member, label, default)",
        "//   FRTX_COLOR(key, member, label, r, g, b)",
        "//   FRTX_SECTION(label)",
        "",
        "#ifndef FRTX_FLOAT",
        "#define FRTX_FLOAT(key, member, label, lo, hi, def, step)",
        "#endif",
        "#ifndef FRTX_INT",
        "#define FRTX_INT(key, member, label, lo, hi, def, step)",
        "#endif",
        "#ifndef FRTX_BOOL",
        "#define FRTX_BOOL(key, member, label, def)",
        "#endif",
        "#ifndef FRTX_COLOR",
        "#define FRTX_COLOR(key, member, label, r, g, b)",
        "#endif",
        "#ifndef FRTX_SECTION",
        "#define FRTX_SECTION(label)",
        "#endif",
        "",
    ]
    for kind, key, member, label, desc, lo, hi, default, step in SPEC:
        if kind == TITLE:
            out.append(f'FRTX_SECTION("{label}")')
        elif kind == BOOL:
            out.append(f'FRTX_BOOL("{key}", {member}, "{label}", {"true" if default else "false"})')
        elif kind == COLOR:
            r, g, b = default
            out.append(f'FRTX_COLOR("{key}", {member}, "{label}", {r}, {g}, {b})')
        elif kind == INT:
            out.append(f'FRTX_INT("{key}", {member}, "{label}", {lo}, {hi}, {default}, {step})')
        else:
            out.append(f'FRTX_FLOAT("{key}", {member}, "{label}", {f(lo)}, {f(hi)}, {f(default)}, {f(step)})')
    out += [
        "",
        "#undef FRTX_FLOAT",
        "#undef FRTX_INT",
        "#undef FRTX_BOOL",
        "#undef FRTX_COLOR",
        "#undef FRTX_SECTION",
        "",
    ]
    pathlib.Path('src/FRTXParams.inc').write_text("\n".join(out))
    return sum(1 for s in SPEC if s[0] != TITLE)


if __name__ == '__main__':
    n, titles = gen_mod_json()
    params = gen_inc()
    print(f"mod.json: {n} settings ({titles} section titles, 1 keybind)")
    print(f"src/FRTXParams.inc: {params} tunable parameters")
