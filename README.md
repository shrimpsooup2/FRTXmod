# FRTX

A [Geode](https://geode-sdk.org) mod that adds RTX-style post processing to
Geometry Dash: bloom, filmic tonemapping and colour grading.

There is no ray tracing here and there cannot be — GD renders through
cocos2d-x on OpenGL ES 2.0, with no depth buffer, no normals and no float
render targets to work with. What this mod does is reproduce the *visual
signature* people read as "RTX" entirely in screen space.

## How it works

```
scene renders  ->  [capture target]
                        |  bright pass + downsample
                   [bloom level 0]  --box-->  [level 1]  --box-->  [level 2]
                        |                        |                    |
                   H+V gaussian             H+V gaussian         H+V gaussian
                        \_______________________ | ___________________/
                                                 v
   capture + weighted bloom -> exposure -> ACES -> white balance -> contrast
                            -> saturation -> vignette -> grain/dither -> screen
```

### Capturing the frame

GD offers no clean "wrap the whole frame" hook. `CCDirector::drawScene` swaps
buffers before we would get a chance to composite, and hooking `CCNode::visit`
would fire for every node in the game.

So instead the mod brackets the gameplay layer with two sibling nodes inside its
scene: a capture node at the lowest possible z-order and a composite node at the
highest. cocos visits children in z-order, so everything drawn in between lands
in our render target.

`src/nodes/FRTXNodes.cpp` holds both ends of that bracket, and everything else
about the technique is documented at the top of
`src/render/FRTXPostProcessor.hpp`. Two constraints are worth knowing before
touching that code:

- **The bracket nodes must keep an identity transform.** `CCRenderTexture`
  pushes matrices in `begin()` and pops them in `end()`, and here those calls
  land in two different `visit()` bodies. The push/pop counts still balance, but
  only an identity transform leaves the modelview the scene renders under
  unchanged.
- **The capture target must stay the same size as the screen.**
  `CCRenderTexture::begin()` rewrites the projection to fit whatever size it is
  handed, so a downscaled capture would rescale the scene rather than sample it
  more coarsely. Only the bloom pyramid is allowed to shrink; that is what
  *Bloom: Resolution Scale* controls.

### Passes

Every pass draws one fullscreen triangle strip whose positions are already in
clip space, so the vertex shader ignores `CC_MVPMatrix` entirely. That makes the
chain immune to whatever projection cocos has set up at the time. Texture
coordinates arrive in normalised screen space and each shader scales them by the
uv extent of the target it reads, which keeps things correct even when a driver
lacks NPOT support and cocos pads a render texture up to a power of two.

## Building

The mod is built with the [Geode CLI](https://docs.geode-sdk.org/getting-started/):

```sh
geode build
```

CI builds it on every push (`.github/workflows/build.yml`) and uploads the
packaged `.geode` as a workflow artifact.

## Platform support

Windows is the supported target and the one listed in `mod.json`. The rendering
code is deliberately portable — GLES2-safe GLSL with no version directive, no
desktop-only GL calls — and CI also builds macOS and Android for information,
but those builds are allowed to fail and the platforms are not shipped yet.

## Roadmap

- [x] Frame capture and composite
- [x] Bloom
- [x] Tonemapping, colour grading, lens effects
- [ ] Object-aware emissive buffer, so the player, glow-enabled objects and
      particles bloom regardless of screen luminance
- [ ] Light rays / god rays
- [ ] In-game settings popup with live sliders and presets
- [ ] Editor support
- [ ] Option to exclude the UI layer from the effect

## Regenerating the icon

`logo.png` is generated rather than drawn, so it stays reproducible:

```sh
python3 tools/make_logo.py
```
