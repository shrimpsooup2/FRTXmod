# Changelog

## v0.9.0

Three bug fixes and one compatibility fix, all from playing it.

- **Light rays were being cancelled by their own occlusion term.** It dimmed
  rays wherever the scene was dark, on the reasoning that a dark pixel is a
  solid object blocking the light. In a 2D game with no depth buffer that
  reasoning does not hold: dark pixels here are mostly empty background, which
  is exactly where shafts should be most visible. Cathedral used 0.75 of it, so
  its rays were cut to a quarter precisely where they would have shown, which
  is why that preset looked like it did nothing. The term is now off by default,
  renamed **Fade Over Dark**, and described as the stylistic choice it is.
- **Rays are much stronger.** The pass normalised by a guess that over-divided
  badly at high sample counts. It now normalises by the geometric series the
  march actually accumulates, so sample count and decay are purely shape and
  intensity alone sets brightness. The intensity range went to 6 and the ray
  presets were retuned around it.
- **Lens flare was invisible, not broken.** Ghosts were multiplied by an extra
  0.25 on top of a squared falloff, so they peaked at a quarter of a bloom
  sample. That factor is gone and the range now reaches 3.
- **New: Bloom Isolation Boost.** The inverse of background suppression, from
  the same neighbourhood measurement: bright things surrounded by darkness glow
  harder, and get more rays with it.
- **Camera effect compatibility.** `CCRenderTexture::begin()` reloads the
  standard projection and identity modelview, and `end()` resets the viewport.
  Wrapping the game layer in those meant every frame stamped over whatever the
  2.2 camera triggers had configured, which is why levels like Dash broke. The
  renderer now binds framebuffers itself and never touches a matrix, a
  projection or the viewport, and it sizes the capture from the viewport the
  game is really drawing into rather than from the design resolution.

## v0.8.0

Light rays get most of the attention, plus three new lens effects and six more
presets.

**Rays**

- **Jittered sampling.** Every pixel used to march from the same distances,
  which is what produces the concentric stepping that gives cheap god rays
  away. Starting each pixel a random fraction of a step along the ray turns
  that banding into fine noise instead, and costs nothing.
- **Occlusion.** Rays now dim where the scene in front of them is dark, so
  shafts pass behind solid objects rather than washing over them. This is most
  of what makes them read as light in the air.
- **Sun disc.** An optional glowing source at the origin, so shafts have
  something to come from.
- **Shimmer.** Strength varies slowly with angle around the source, so the fan
  breathes instead of sitting still.
- **Off-screen origins.** The origin range now extends past the screen edges,
  which is what makes near-parallel shafts possible.

**New effects**

- **Lens flare ghosts**, mirrored through the screen centre.
- **Halation**, a warm red bleed driven from the widest blur level. Not more
  bloom: wide, soft and strongly coloured.
- **Lens distortion**, barrel or pincushion.

**Presets** now number sixteen, with a third row built around the ray pass:
Dawn, Cathedral, Eclipse, Aurora and Inferno, plus Retro. Shift plus a number
key reaches the second bank of ten in the tuner.

`tools/check.py` also now fails if a button setting has no listener, since a
generated roster makes it easy to add buttons that quietly do nothing.

## v0.7.0

- **Fixes tuner navigation.** cocos' `enumKeyCodes` carries two arrow families:
  `KEY_Up`/`Down`/`Left`/`Right`, which are the Windows virtual key codes and
  what the input layer actually sends, and `KEY_ArrowUp` and friends at 0x11B,
  which nothing on Windows produces. The tuner matched only the second set, so
  it opened but could not be navigated at all. It now accepts both, and
  `tools/check.py` fails if the real codes ever stop being handled.
- **Six style presets**: Neon, Cinematic, Dreamy, Noir, Vivid and Sunbeam,
  alongside the four intensity presets. The roster is generated, so the buttons,
  the `FRTXPreset` enum and the tuner's number keys cannot disagree.
- **Background suppression.** Saturation bias does not separate a glowing object
  from a bright *saturated* backdrop, which is why bright backgrounds could
  bloom as hard as the objects in front of them. The bright pass now also
  weights by how much brighter a pixel is than a wide, screen-scale
  neighbourhood. The neighbourhood is deliberately large so a big glowing object
  is still small compared to it and does not get hollowed out.
- Tuner gains Page Up/Down and Home/End, a status line confirming a preset was
  applied, and number keys for all ten presets.

## v0.6.0

**Fixes settings not working.** The preset was a value that got re-applied on
every read, and it defaulted to Showcase, so out of the box almost every slider
in the settings menu did nothing at all: you moved it, and the preset stamped
over your value again immediately. That was a design mistake, not a small bug.

- **Presets are now buttons that write their values in once.** Subtle,
  Showcase, Overkill and Performance appear at the top of the settings. Nothing
  overrides anything at read time any more, so every control is live at all
  times. Colour tints and the master switches are left alone by presets.
- `mod.json`'s defaults are the Showcase values, and `tools/check.py` now
  asserts that, so a fresh install and a Showcase press cannot diverge.
- **Presets from the tuner** too: keys 1-4.
- **The tuner can no longer be built by a stray keypress.** Its keyboard
  listener runs for every key in the game and used to construct the panel on
  the first one, which could happen before the game's fonts were loaded. It now
  only handles keys once the panel exists, every label creation is checked, and
  a failed build disables the overlay instead of crashing.
- A quick-start block at the top of the settings explains the F8 tuner.

## v0.5.0

Performance pass. The per-frame pixel work is irreducible — the glow depends on
what is on screen this frame — but everything around it was being repeated for
no reason.

- **Warm-up at level open.** Shader compilation and render target allocation
  move from the first captured frame to `PlayLayer::init` and
  `LevelEditorLayer::init`, so the cost lands during the transition rather than
  as a hitch once the player is already moving.
- **Settings are cached.** Forty-five `getSettingValue` calls a frame, each a
  hash lookup plus a `dynamic_cast`, are now one snapshot invalidated by a
  settings-changed listener.
- **Bloom weights** are recomputed on a generation counter rather than every
  frame.
- **Disabled effects cost nothing.** Unused bloom levels, streaks and rays were
  still being sampled at full resolution and multiplied by zero; those fetches
  are now behind uniform branches.
- **Clarity taps are configurable**, 4 or 8, defaulting to 4. It is the most
  expensive part of the final pass and 4 is near indistinguishable at the radii
  clarity is actually used at.
- **New Performance preset** (4), with everything that costs fill rate turned
  off and smaller bloom buffers.

## v0.4.0

Verified against the real Geode SDK and GD bindings for the first time, which
corrected some guesses and unlocked the rest of the roadmap. First version to
have actually been compiled.

- **Now targets Geode 5.10.1 and GD 2.2081.** The previous `4.0.0` / `2.2074`
  would not have loaded.
- **Frame capture is now a single hook on `GJBaseGameLayer::visit()`**, which
  replaces the two-node z-order sandwich entirely. `PlayLayer::update` does not
  exist in the bindings and never would have compiled; `visit()` does, and both
  `PlayLayer` and `LevelEditorLayer` inherit it, so the begin/end pair now lives
  in one function and editor support comes free.
- **Live in-game tuner** on F8, rebindable. Lists all 44 controls and edits them
  while the game runs, swallowing the arrow keys so tuning does not make you
  jump.
- **Light rays**: a radial march from a fixed point or from the player, with
  density, decay, weight, sample count and tint.
- **Editor support**, off by default.
- **Option to lift the UI out of the effect**, so the attempt counter and
  progress bar stay crisp.
- **Per-effect colour tints** for bloom, streaks and rays; **vignette roundness
  and softness**.
- **Settings are generated from one spec.** `tools/gen_settings.py` produces
  both `mod.json` and the X-macro list the config reader and tuner are built
  from, so 52 settings cannot drift apart. `tools/check.py` verifies that, plus
  every default's range and every shader uniform, and CI runs it before the
  build.

## v0.3.0

Retuned against an actual frame of an RTX showcase level, which contradicted
several v0.2.0 defaults.

- **Streaks off by default.** Showcase glows are radially symmetric; there is no
  anamorphic streaking in the reference. The feature stays, Overkill still uses
  it, but it is no longer part of the intended look.
- **Neutral grade.** Temperature and highlight tone are now 0. The reference is
  monochrome cold blue and the level's own palette carries the image, so a warm
  grade was fighting it. Shadow tone stays slightly cool.
- **Clarity dropped to 0.12 at a 3.5px radius**, and the unsharp difference is
  now clamped. At the old 0.40 / 6px it would have ringed every neon outline
  with a dark halo, which the reference plainly does not have.
- **New Bloom: Spread control** for how fast the per level weights fall off,
  which is what decides how large a halo reads. Showcase uses 0.8 for the big
  soft falloff the reference has around every light source.
- Wider, lower-threshold bloom to match: intensity 1.6, threshold 0.52,
  radius 2.0. Black point raised to 0.05, vignette and grain nearly removed.

## v0.2.0

Chasing the look of RTX level showcases rather than a generic bloom filter.

- **Emissive bias** on the bright pass: the glow is weighted towards saturated
  colour, so neon and glow objects bloom while bright backgrounds do not.
  Pure whites are exempted so white glow still works.
- **Anamorphic streaks**: three chained horizontal blurs off the bright pass,
  with a faint cool tint.
- **Clarity**: local contrast via an unsharp mask against a wide ring of taps.
- **Black point** and **split toning** (cool shadows, warm highlights).
- **Presets**: Subtle, Showcase and Overkill, plus Custom for the sliders.
  Showcase is the new default.
- Debug view 3 shows the streak buffer on its own.

## v0.1.0

Initial release.

- Full-frame capture and composite pipeline.
- Bloom: soft-knee bright pass, up to three blur levels, adjustable radius,
  intensity and buffer resolution.
- ACES filmic tonemapping.
- Colour grading: exposure, contrast, saturation, temperature, tint.
- Lens effects: vignette, chromatic aberration, film grain, dithering.
- Debug views for tuning the bloom threshold.
