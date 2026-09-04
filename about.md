# FRTX

**RTX-style post processing for Geometry Dash.**

Geometry Dash draws through OpenGL ES 2.0, so there is no ray tracing here and
there could not be. What this mod does is rebuild the *look* of an RTX showcase
in screen space: the game is captured into a buffer, the bright parts are
pulled out and blurred into a glow, light shafts are marched out of them, and
the result is graded before it reaches your screen.

## Getting started

You do not need to touch the sliders. A fresh install already looks like the
**Showcase** preset. If you want something different, press one of the preset
buttons at the top of the settings and it writes that preset's values in for
you. Nothing is locked afterwards — a preset is a starting point, and every
slider stays editable.

If it costs you frames, press **Performance**. If a level looks wrong, press
**Showcase** to get back to a known state.

## The live tuner

Press **O** while playing a level. An overlay lists every setting, and you can
change them while the game runs, which is far easier than guessing in a menu.
The key is rebindable in the settings.

- **Up / Down** — move between settings
- **Left / Right** — change the selected value
- **Shift + Left / Right** — coarse, ten times the step
- **Alt + Left / Right** — fine, a tenth of the step
- **Page Up / Page Down** — jump a screenful
- **Home / End** — first or last setting
- **R** — reset the selected value to its default
- **0 - 9** — apply a preset; hold **Shift** for the second ten
- **Escape** — close

Arrow keys are swallowed while the panel is open, so tuning does not also make
you jump.

## Presets

**By intensity**

- **Subtle** — the look without the drama. Fine for actually playing.
- **Showcase** — what these levels look like in a video. The default.
- **Overkill** — too much, on purpose.
- **Performance** — for keeping frames. Everything that costs fill rate off.

**By style**

- **Neon** — for glow-heavy levels. Only strongly saturated things bloom.
- **Cinematic** — filmic warmth, cool shadows, heavy vignette and grain.
- **Dreamy** — a wide, soft haze instead of a glow.
- **Noir** — monochrome, hard contrast, deep vignette.
- **Vivid** — punch from colour and contrast rather than from glow.
- **Retro** — cheap optics on purpose: fringing, curvature and grain.

**Built around light rays**

- **Sunbeam** — a straightforward sun with visible shafts.
- **Dawn** — a low warm sun throwing long shafts across the frame.
- **Cathedral** — hard shafts dropping from above the screen.
- **Eclipse** — a dark frame around a fierce, tight core.
- **Aurora** — cool and hazy, with the shimmer doing the work.
- **Inferno** — a hot close sun and heavy film halation.

## What each setting does

### Bloom

The glow around bright things.

- **Intensity** — how strongly the glow is added back on top.
- **Threshold** — how bright something must be before it glows at all.
- **Soft Knee** — how gradually things fade in as they cross the threshold.
- **Emissive Bias** — biases the glow towards *saturated* colour, so neon and
  glow objects bleed light while washed-out backgrounds do not. Pure white is
  exempt, because a lot of GD glow is white.
- **Background Suppression** — holds back glow in areas whose surroundings are
  already bright. Raise this when a bright sky blooms as hard as the objects in
  front of it.
- **Isolation Boost** — the opposite: a bright thing surrounded by darkness
  glows harder than the same brightness would in a bright area. Because light
  rays are built from the glow, this gives isolated objects more rays too.
- **Background Scale** — how wide a neighbourhood those two measure against.
  Large values only affect screen-scale washes. If a big glowing object starts
  looking hollow in the middle, raise this.
- **Radius** — how far the blur reaches.
- **Spread** — how *large* the halo reads, as opposed to how bright.
- **Levels** — how many blur sizes are mixed. Fewer is cheaper.
- **Resolution Scale** — the size of the blur buffers. The single biggest
  performance control.
- **Tint** — colours the glow. White keeps whatever colour is glowing.

### Light Rays

Shafts of light marched outward from a point.

- **Intensity** — the only brightness control; everything else is shape.
- **Density** — how far along the ray the samples are spread. Longer shafts.
- **Decay** — how fast a ray fades as it travels.
- **Weight** — the contribution of each step.
- **Samples** — how many steps. The main cost of this pass.
- **Jitter** — randomises where each pixel starts sampling. Without it the
  shafts band into visible concentric arcs. Leave it high.
- **Origin / Origin X / Origin Y** — where the light is. Values outside 0 to 1
  put it off screen, which is what makes shafts look near-parallel instead of
  fanning out of a point. Origin can also follow the player.
- **Sun Disc / Sun Size** — draws a glowing source at the origin. It only
  appears where the level actually has light there, so it fades in and out
  rather than sitting in one spot the whole time. Off by default.
- **Shimmer** — slowly varies ray strength with angle, like disturbed air.
- **Fade Over Dark** — dims rays over dark parts of the screen. This is a
  stylistic choice, not a physical one: the mod cannot tell a solid object from
  empty background, and in this game the dark areas are usually empty, which is
  exactly where shafts should show. Leave at 0 unless you want the opposite.

### Anamorphic Streaks

Horizontal light streaks off bright points, the way a cinema lens flares. Off
by default, because showcase levels tend to glow radially rather than streak.

### Lens Flare

- **Ghosts** — mirrored copies of bright areas through the centre of the
  screen, the way light bounces between lens elements.
- **Ghost Spacing** — how far apart they sit.
- **Halation** — a warm red bleed around highlights, the way film scatters
  light back through its own base. Wider and far more coloured than bloom.
- **Distortion** — barrel curvature at positive values, pincushion at negative.

### Clarity

Local contrast. Lifts edge and material detail so a flat 2D frame reads as
though it has depth. Keep it gentle: pushed hard it puts a dark ring around
every glowing outline, which is the giveaway of a sharpening filter. **Taps**
trades quality for speed and is the most expensive thing in the final pass.

### Grade

- **Filmic Tonemapping** — rolls highlights off smoothly instead of clipping.
- **Exposure** — overall brightness, applied before the tonemap.
- **Black Point** — crushes the darkest values to true black. A little of this
  is most of what separates graded footage from a filter.
- **Contrast**, **Saturation** — the usual.
- **Shadow Tone / Highlight Tone** — tints shadows and highlights separately.
  Cool shadows with warm highlights is the classic cinematic split.
- **Temperature / Tint** — white balance for the whole image.

### Lens

Vignette with roundness and softness, chromatic aberration, film grain, and
dithering to hide colour banding in smooth gradients.

## If something looks wrong

- **The background glows as much as the objects** — raise **Bloom: Background
  Suppression**, or raise **Bloom: Threshold**.
- **A big glowing object looks hollow** — raise **Bloom: Background Scale**.
- **Dark rings around neon outlines** — lower **Clarity: Amount**.
- **Rays look like concentric arcs** — raise **Light Rays: Jitter**.
- **It costs too many frames** — press **Performance**, or lower **Bloom:
  Resolution Scale**, then **Clarity** to 0, then **Bloom: Levels**.

**Debug View** is the fastest way to tell what a setting is doing: `1` shows the
captured scene alone, `2` the bloom alone, `3` the streaks, `4` the rays, and
`5` the flare and halation.

## Compatibility

The effect wraps the whole gameplay layer without touching the projection, the
modelview or the viewport, so 2.2 camera triggers and shader triggers keep
working. Turning **Enabled** off leaves the game rendering completely untouched
with no GPU memory used at all.
