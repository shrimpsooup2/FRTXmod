# FRTX

**RTX-style post processing for Geometry Dash.**

Geometry Dash runs on OpenGL ES 2.0 through cocos2d-x, so there is no ray
tracing happening here and there never could be. What FRTX does instead is
rebuild the *look* people recognise as "RTX" with a screen-space post-processing
chain: the whole frame is captured into a render target, bright areas are
extracted and blurred into a glow, and the result is tonemapped and graded
before it reaches your screen.

## What it does

- **Bloom** — bright objects, glow-enabled blocks and particles bleed light into
  their surroundings, through a three-level blur pyramid with a soft threshold.
- **Filmic tonemapping** — an ACES curve so highlights roll off smoothly
  instead of clipping to flat white.
- **Colour grading** — exposure, contrast, saturation, temperature and tint.
- **Lens effects** — vignette, chromatic aberration, film grain and dithering.

Everything is adjustable, and every part can be turned off on its own. If you
disable all of it the game renders completely untouched and no GPU memory is
used at all.

## Performance

The bloom buffers default to half resolution, which is where nearly all of the
cost lives. If you need frames back, drop **Bloom: Resolution Scale** or
**Bloom: Quality** before you turn anything else off.

## Tuning tips

Set **Debug View** to `2` to see only the bloom. That makes it much easier to
pick a **Bloom: Threshold** that catches the things you want glowing and leaves
everything else alone.
