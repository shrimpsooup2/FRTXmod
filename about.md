# FRTX

**RTX-style post processing for Geometry Dash.**

Geometry Dash runs on OpenGL ES 2.0 through cocos2d-x, so there is no ray
tracing happening here and there never could be. What FRTX does instead is
rebuild the *look* of an RTX showcase in screen space: the game is captured into
a render target, emissive-looking areas are extracted and blurred into a glow,
and the result is graded before it reaches your screen.

## Start here

A fresh install already looks like **Showcase**. The preset buttons at the top
of the settings write their values in for you:

- **Subtle**: the look without the drama. Safe for actually playing.
- **Showcase**: what these levels look like in a video.
- **Overkill**: too much, on purpose.
- **Performance**: for keeping frames. Smaller bloom buffers, one fewer blur
  level, and everything that costs fill rate switched off.

A preset is a starting point, not a lock. It writes its values into the
settings once, and every slider stays editable afterwards.

## Tune it live

Press **F8** in a level to open an overlay listing all 44 controls, and adjust
them while the game is running. The key is rebindable.

| Key | Does |
|---|---|
| Up / Down | move between settings |
| Left / Right | adjust the selected value |
| Shift | coarse, 10x the step |
| Alt | fine, a tenth of the step |
| R | reset the selected value |
| 1 - 4 | apply Subtle / Showcase / Overkill / Performance |
| Escape | close |

Arrow keys are swallowed while the panel is open, so tuning does not also make
you jump.

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

Post processing cannot be precomputed: the glow depends on what is on screen
this frame, and the screen changes every frame. What *can* be done once is
everything around it, and is: shaders are compiled and buffers allocated when
the level opens rather than on the first frame you see, settings are read once
and cached rather than forty-five times a frame, and effects that are switched
off cost nothing at all rather than being sampled and multiplied by zero.

For the per-frame cost that remains, **Preset 4 (Performance)** is the one
setting to reach for. Tuning by hand, in order of how much each buys:

1. **Bloom: Resolution Scale** — the single biggest lever.
2. **Clarity** to 0 — it is four full resolution fetches per pixel, or eight
   if you raised **Clarity: Taps**.
3. **Bloom: Quality** — one fewer level is two fewer blur passes.
4. **Lens: Chromatic Aberration** to 0 — two more full resolution fetches.
5. **Light Rays: Samples**, if you turned rays on at all.

## Tuning tips

Use **Debug View** while you tune: `2` shows only the bloom, `3` only the
streaks, `4` only the rays. It makes it obvious whether **Bloom: Threshold** and
**Bloom: Emissive Bias** are catching the things you actually want glowing.
