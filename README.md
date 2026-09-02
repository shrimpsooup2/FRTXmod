# FRTX

A [Geode](https://geode-sdk.org) mod that adds RTX-style post processing to
Geometry Dash: emissive bloom, anamorphic streaks, light rays, local contrast
and cinematic colour grading — with an in-game tuner for adjusting all of it
while you play.

There is no ray tracing here and there cannot be. GD renders through cocos2d-x
on OpenGL ES 2.0, with no depth buffer, no normals and no float render targets.
What this mod does is reproduce the *visual signature* people read as "RTX"
entirely in screen space.

Targets Geode 5.10.1 and GD 2.2081 on Windows.

## How it works

```
GJBaseGameLayer::visit()
  beginCapture()  ->  [capture target]
      ... the whole game layer renders into it ...
  endCapture()
        |  bright pass, saturation weighted + downsample
   [bloom level 0] --box--> [level 1] --box--> [level 2]
        |     \       \            |                 |
   H+V gaussian \       \     H+V gaussian      H+V gaussian
        |        \       \_ radial march from the light origin -> [rays]
        |    3x horizontal-only blur, steps 1/4/16 -> [streak]
        \_________________ | ______|_________________|
                           v
   capture -> clarity -> + bloom + streak + rays -> exposure -> ACES
           -> black point -> white balance -> split tone -> contrast
           -> saturation -> vignette -> grain/dither -> screen
```

### Capturing the frame

GD offers no clean "wrap the whole frame" hook: `CCDirector::drawScene` swaps
buffers before a composite would be possible, and `CCNode::visit` would fire for
every node in the game.

`GJBaseGameLayer::visit()` is the answer. It is a real virtual override in the
bindings, and both `PlayLayer` and `LevelEditorLayer` inherit it — so a single
hook captures exactly the game, in one function, with the render target's
`begin()`/`end()` pair trivially balanced, and editor support comes free.

Two constraints are worth knowing before touching `src/render/`:

- **The capture target must match the screen size.**
  `CCRenderTexture::begin()` rewrites the projection to fit whatever size it is
  handed, so a downscaled capture would rescale the scene rather than sample it
  more coarsely. Only the bloom pyramid shrinks; that is what *Bloom:
  Resolution Scale* controls.
- **Passes must not depend on cocos' projection.** Every pass draws a
  fullscreen quad whose positions are already in clip space and ignores
  `CC_MVPMatrix`. Texture coordinates arrive in normalised screen space and each
  shader scales them by the uv extent of the target it reads, which stays
  correct when a driver lacks NPOT support and cocos pads a render texture up to
  a power of two.

### Matching the showcase look

Four things separate this from a generic bloom filter, and they matter more than
bloom strength does:

- **The bright pass is weighted by saturation.** GD's neon, glow objects and
  particles are strongly saturated; skies and background gradients are bright
  but washed out. Weighting by saturation is the closest thing to object-aware
  emission available without reading GD's object data, and it is what stops the
  whole background from glowing. Pure whites are exempted, since a lot of GD
  glow is white.
- **Halo width is its own control.** How fast the per level bloom weights fall
  off decides how *large* the glow reads, independently of how *bright* it is.
  Showcase footage has big soft halos, which needs a flat falloff, not more
  intensity.
- **Clarity, clamped.** An unsharp mask against a ring of wide taps lifts local
  contrast, but the difference is clamped before it is added back: otherwise the
  dark side of a bright edge is pushed darker still and every glowing outline
  picks up a dark ring, the artefact that gives a sharpening filter away.
- **A grade that stays out of the way.** These levels carry their own palette.
  The default grade is near-neutral by design; only the shadows are cooled.

## The live tuner

Press **F8** in a level (rebindable) for an overlay listing every setting, and
adjust them while the game runs.

| Key | Does |
|---|---|
| Up / Down | move between settings |
| Left / Right | adjust the selected value |
| Shift + Left/Right | coarse, 10x the step |
| Alt + Left/Right | fine, a tenth of the step |
| R | reset the selected value to its default |
| 1 - 4 | apply Subtle / Showcase / Overkill / Performance |
| Escape | close |

Arrow keys are swallowed while the panel is open, so adjusting a slider does not
also make the player jump.

## Presets

Four buttons at the top of the settings: Subtle, Showcase, Overkill,
Performance. Pressing one **writes its values into the settings**; it does not
install an override.

That distinction is the whole point. Presets used to be a value that
`FRTXConfig::read()` re-applied on every read, which meant that with the default
preset active — which it was, out of the box — dragging almost any slider in the
settings menu silently did nothing. An override layer that quietly wins over the
user's own input is worse than having no presets at all. Now `mod.json`'s
defaults are the Showcase values, a preset press stamps values in, and every
control is live at all times. `tools/check.py` asserts the defaults still equal
the Showcase preset, so a fresh install and a Showcase press cannot diverge.

## Performance

The pixel work is irreducibly per-frame — bloom depends on what is on screen
now — but everything around it happens once:

- **Shaders compile and buffers allocate at level open**, from `PlayLayer::init`
  and `LevelEditorLayer::init`, rather than lazily on the first captured frame.
  Five shader compiles and eight framebuffer allocations inside the first frame
  the player sees is exactly where a hitch is most visible.
- **Settings are cached.** `Mod::getSettingValue` is a hash lookup plus a
  `dynamic_cast` per call, and there are forty-five of them; reading that every
  frame was pure waste. `FRTXConfig::current()` returns a snapshot that a
  `listenForAllSettingChanges` listener invalidates. Geode dispatches that event
  synchronously from `setValue`, so both the settings menu and the tuner are
  covered without polling.
- **Derived values follow the cache**, not the frame. A generation counter lets
  the bloom weights be recomputed only when a setting actually moved.
- **Effects that are off cost nothing.** An unused bloom level or a disabled
  streak still had a texture bound to its unit and was being sampled and
  multiplied by zero; those fetches are now behind uniform branches, which are
  coherent across the draw and so free when taken.

## Settings are generated

There are 52 settings, and `mod.json`, `FRTXConfig`'s fields and the tuner's
table all have to agree about every one of them. Rather than maintain that by
hand, `tools/gen_settings.py` holds the spec and generates `mod.json` plus
`src/FRTXParams.inc`, an X-macro list the config reader and the tuner are both
built from. To add or change a setting, edit the spec and re-run it:

```sh
python3 tools/gen_settings.py
python3 tools/check.py
```

`tools/check.py` verifies the generated files are current, that every default
sits inside its own range, and that every shader uniform the C++ sets exists in
the shader it targets with a matching component count. CI runs it before the
build, because none of it needs a compiler and all of it would otherwise only
surface minutes later on a Windows runner.

## Building

```sh
geode build
```

CI builds on every push and uploads the packaged `.geode` as a workflow
artifact.

## Platform support

Windows is the supported target and the only one listed in `mod.json`.

CI builds Windows alone, on purpose. Adding macOS and Android jobs "for
information" does not work: they fail during CMake configure with
`JSON member 'gd mac' not found` before a single source file is compiled, so
they report nothing about whether the code is portable. To actually add a
platform, add it to `gd` in `mod.json` and add a job to the workflow.

The rendering code is written to be portable regardless — GLES2-safe GLSL with
no version directive, no desktop-only GL calls — so that remains a small change
rather than a rewrite.

## Regenerating the icon

`logo.png` is generated rather than drawn, so it stays reproducible:

```sh
python3 tools/make_logo.py
```

## Roadmap

- [x] Frame capture and composite
- [x] Bloom, tonemapping, colour grading, lens effects
- [x] Emissive-biased bloom, anamorphic streaks, clarity, split toning
- [x] Presets
- [x] Light rays
- [x] Editor support
- [x] Option to lift the UI layer out of the effect
- [x] In-game live tuner
- [ ] True object-aware emissive buffer, driven by GD's glow data rather than by
      screen saturation
- [ ] Save and load named custom presets
