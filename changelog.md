# Changelog

## v0.4.0

Verified against the real Geode SDK and GD bindings for the first time, which
corrected some guesses and unlocked the rest of the roadmap.

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
