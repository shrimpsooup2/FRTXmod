# FRTX

**RTX-style post processing for Geometry Dash.**

Geometry Dash runs on OpenGL ES 2.0 through cocos2d-x, so there is no ray
tracing happening here and there never could be. What FRTX does instead is
rebuild the *look* of an RTX showcase in screen space: the game is captured into
a render target, emissive-looking areas are extracted and blurred into a glow,
and the result is graded before it reaches your screen.

## Start here

Set **Preset** and leave everything else alone:

- **1 — Subtle**: the look without the drama. Safe for actually playing.
- **2 — Showcase** *(default)*: what these levels look like in a video.
- **3 — Overkill**: too much, on purpose.
- **0 — Custom**: every slider is yours.

## Tune it live

Press **F8** in a level to open an overlay listing all 44 controls, and adjust
them while the game is running. Arrow keys move and adjust, Shift is coarse, Alt
is fine, R resets a value, Escape closes. The key is rebindable.

Arrow keys are swallowed while the panel is open, so tuning does not also make
you jump. Editing anything while a preset is active switches to Custom, because
otherwise the preset would just overwrite your change.

## What it does

- **Emissive bloom** — the bright pass is biased towards saturated colour, so
  neon, glow objects and particles bleed light while a bright background stays
  where it is. Pure white still glows, because plenty of GD glow is white.
- **Light rays** — shafts radiating from a fixed point or from the player.
- **Anamorphic streaks** — horizontal light streaks, the way a cinema lens
  flares. Off in Showcase, since showcase levels glow radially; Overkill uses
  them.
- **Clarity** — local contrast that lifts edge and material detail, kept
  deliberately gentle. Pushed hard it rings every neon outline with a dark halo.
- **Filmic tonemapping and grading** — an ACES curve, a crushed black point,
  and split toning. The default grade is near-neutral on purpose: these levels
  carry their own palette and a heavy grade fights it.
- **Lens effects** — vignette with roundness and softness, chromatic
  aberration, film grain and dithering.

Everything can be turned off on its own. If you disable all of it the game
renders completely untouched and no GPU memory is used at all.

## Performance

The bloom buffers default to half resolution, which is where most of the cost
lives. If you need frames back, in order: drop **Bloom: Resolution Scale**, then
**Bloom: Quality**, then **Light Rays: Samples**, then **Clarity** to 0.

## Tuning tips

Use **Debug View** while you tune: `2` shows only the bloom, `3` only the
streaks, `4` only the rays. It makes it obvious whether **Bloom: Threshold** and
**Bloom: Emissive Bias** are catching the things you actually want glowing.
