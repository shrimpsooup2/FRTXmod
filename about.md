# FRTX

**RTX-style post processing for Geometry Dash.**

Geometry Dash runs on OpenGL ES 2.0 through cocos2d-x, so there is no ray
tracing happening here and there never could be. What FRTX does instead is
rebuild the *look* of an RTX showcase with a screen-space post-processing chain:
the whole frame is captured into a render target, emissive-looking areas are
extracted and blurred into a glow, and the result is graded before it reaches
your screen.

## Presets

Start with **Preset**, not with the sliders:

- **1 — Subtle**: the look without the drama. Safe for actually playing.
- **2 — Showcase** *(default)*: what these levels look like in a video.
- **3 — Overkill**: too much, on purpose.
- **0 — Custom**: use every slider below by hand.

Any preset other than Custom overrides every look setting, so switch to Custom
before tuning.

## What it does

- **Emissive bloom** — the bright pass is biased towards saturated colour, so
  neon, glow objects and particles bleed light while a bright background stays
  where it is. Pure white still glows, because plenty of GD glow is white.
- **Anamorphic streaks** — horizontal light streaks off bright points, the way a
  cinema lens flares. Off in the Showcase preset, because showcase levels glow
  radially rather than streaking; Overkill turns them on.
- **Clarity** — local contrast that lifts edge and material detail, kept
  deliberately gentle. Pushed hard it rings every neon outline with a dark
  halo, which is the giveaway of a sharpening filter.
- **Filmic tonemapping and grading** — an ACES curve, a crushed black point,
  and split toning. The default grade is close to neutral on purpose: these
  levels carry their own palette and a heavy grade fights it.
- **Lens effects** — vignette, chromatic aberration, film grain and dithering.

Everything can be turned off on its own. If you disable all of it the game
renders completely untouched and no GPU memory is used at all.

## Performance

The bloom buffers default to half resolution, which is where most of the cost
lives. If you need frames back, in order: drop **Bloom: Resolution Scale**, then
**Bloom: Quality**, then **Clarity** to 0. Streaks are already off unless you
turned them on.

## Tuning tips

Use **Debug View** while you tune. `2` shows only the bloom and `3` only the
streaks, which makes it obvious whether **Bloom: Threshold** and **Bloom:
Emissive Bias** are catching the things you actually want glowing.
