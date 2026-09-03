#!/usr/bin/env python3
"""Consistency checks for FRTX.

None of this needs a compiler, so it catches the classes of mistake that would
otherwise only surface on a Windows CI runner minutes later:

  1. mod.json and src/FRTXParams.inc are still what tools/gen_settings.py
     produces, so nobody has hand-edited a generated file.
  2. Every setting default sits inside its own min/max, and matches the
     Showcase preset so a fresh install and a Showcase press agree.
  3. Every uniform the C++ sets exists in the shader it targets, and vice
     versa, with matching component counts.
  4. Every GLSL blob is brace and paren balanced.
  5. The tuner matches the arrow key codes the input layer really sends.

Run from the repo root:  python3 tools/check.py
"""

import json
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
problems = []


def check_generated_files_are_current():
    """Re-run the generator in a scratch copy and diff the result."""
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        (tmp / 'tools').mkdir()
        (tmp / 'src').mkdir()
        shutil.copy(ROOT / 'tools/gen_settings.py', tmp / 'tools/gen_settings.py')
        shutil.copy(ROOT / 'mod.json', tmp / 'mod.json')

        result = subprocess.run(
            [sys.executable, 'tools/gen_settings.py'], cwd=tmp,
            capture_output=True, text=True)
        if result.returncode != 0:
            problems.append(f"gen_settings.py failed: {result.stderr.strip()}")
            return

        for name in ('mod.json', 'src/FRTXParams.inc'):
            if (tmp / name).read_text() != (ROOT / name).read_text():
                problems.append(
                    f"{name} is out of date -- run: python3 tools/gen_settings.py")


def check_setting_defaults():
    mod = json.loads((ROOT / 'mod.json').read_text())
    for key, spec in mod['settings'].items():
        if 'min' in spec and not (spec['min'] <= spec['default'] <= spec['max']):
            problems.append(
                f"setting '{key}': default {spec['default']} outside "
                f"[{spec['min']}, {spec['max']}]")


def check_defaults_match_showcase():
    """A fresh install must look exactly like pressing the Showcase button.

    Presets no longer override anything at read time -- they write values into
    the settings once -- so mod.json's defaults are the only thing deciding how
    the mod looks before the user touches it. If they drift from the Showcase
    preset, a new install and a Showcase press produce different pictures.
    """
    cpp = (ROOT / 'src/FRTXConfig.cpp').read_text()
    inc = (ROOT / 'src/FRTXParams.inc').read_text()
    mod = json.loads((ROOT / 'mod.json').read_text())

    key_for_member = {
        member: key for key, member in
        re.findall(r'FRTX_(?:FLOAT|INT|BOOL)\("([^"]+)",\s*(\w+),', inc)
    }

    body = re.search(r'void presetInto\(.*?\n    \}\n', cpp, re.S)
    if not body:
        problems.append("could not find presetInto() to check preset defaults")
        return
    body = body.group(0)

    shared = body.split('switch (preset)')[0]
    showcase = re.search(r'case FRTXPreset::Showcase:(.*?)return;', body, re.S)
    if not showcase:
        problems.append("could not find the Showcase preset")
        return

    assignments = {}
    for chunk in (shared, showcase.group(1)):
        for member, value in re.findall(r'cfg\.(\w+)\s*=\s*([^;]+);', chunk):
            assignments[member] = value.strip()

    for member, raw in assignments.items():
        key = key_for_member.get(member)
        if key is None:
            continue  # not a generated setting (colours, etc)
        spec = mod['settings'].get(key)
        if spec is None:
            problems.append(f"preset sets {member} but '{key}' is not in mod.json")
            continue
        if raw in ('true', 'false'):
            wanted = (raw == 'true')
            if spec['default'] is not wanted:
                problems.append(
                    f"'{key}': mod.json default {spec['default']} != Showcase {wanted}")
        else:
            wanted = float(raw.rstrip('f'))
            if abs(float(spec['default']) - wanted) > 1e-6:
                problems.append(
                    f"'{key}': mod.json default {spec['default']} != Showcase {wanted}")


def check_tuner_uses_real_arrow_keys():
    """The tuner must match the arrow codes the input layer actually sends.

    cocos' enumKeyCodes carries two arrow families: KEY_Up/Down/Left/Right,
    which are the Windows virtual key codes, and KEY_ArrowUp and friends at
    0x11B, which nothing on Windows produces. v0.6.0 shipped matching only the
    second set, so the tuner opened but could not be navigated at all.
    """
    tuner = (ROOT / 'src/ui/FRTXTuner.cpp').read_text()
    for key in ('KEY_Up', 'KEY_Down', 'KEY_Left', 'KEY_Right'):
        if f'case {key}:' not in tuner:
            problems.append(
                f"tuner does not handle {key}; the arrow keys the Windows input "
                f"layer sends would be ignored")


def check_preset_buttons_are_listened_for():
    """Every button setting must have a listener, or its buttons do nothing.

    The roster is generated, so it is easy to add a new button group to
    mod.json and forget that the press has to be subscribed to separately.
    """
    mod = json.loads((ROOT / 'mod.json').read_text())
    tuner = (ROOT / 'src/ui/FRTXTuner.cpp').read_text()
    for key, spec in mod['settings'].items():
        if spec.get('type') == 'button' and f'"{key}"' not in tuner:
            problems.append(
                f"button setting '{key}' has no ButtonSettingPressedEventV3 listener; "
                f"its buttons would do nothing")


def check_shader_uniforms():
    shaders = (ROOT / 'src/render/FRTXShaders.hpp').read_text()
    cpp = (ROOT / 'src/render/FRTXPostProcessor.cpp').read_text()

    blobs = dict(re.findall(r'char const\* (\w+) = R"\((.*?)\)";', shaders, re.S))
    if not blobs:
        problems.append("no GLSL blobs found in FRTXShaders.hpp")
        return

    for name, body in blobs.items():
        if body.count('{') != body.count('}'):
            problems.append(f"shader {name}: unbalanced braces")
        if body.count('(') != body.count(')'):
            problems.append(f"shader {name}: unbalanced parens")

    programs = {
        'm_prefilter': 'PREFILTER',
        'm_downsample': 'DOWNSAMPLE',
        'm_blur': 'BLUR',
        'm_rayProgram': 'RAYS',
        'm_composite': 'COMPOSITE',
    }
    declared = {
        name: set(re.findall(r'^\s*uniform\s+\w+\s+(\w+)\s*;', body, re.M))
        for name, body in blobs.items()
    }
    used = {
        shader: set(re.findall(re.escape(member) + r'\.set\w+\("([^"]+)"', cpp))
        for member, shader in programs.items()
    }
    # These are set through a name table rather than as string literals.
    used['COMPOSITE'] |= set(re.findall(r'"(u_bloomUV\d)"', cpp))

    for shader, names in used.items():
        for name in sorted(names - declared.get(shader, set())):
            problems.append(f"{shader}: C++ sets '{name}', shader never declares it")
        for name in sorted(declared.get(shader, set()) - names):
            problems.append(f"{shader}: declares '{name}', C++ never sets it")

    sizes = {'float': 'set1f', 'vec2': 'set2f', 'vec3': 'set3f',
             'vec4': 'set4f', 'sampler2D': 'set1i'}
    for shader, body in blobs.items():
        member = next((m for m, s in programs.items() if s == shader), None)
        if not member:
            continue
        for typ, name in re.findall(r'^\s*uniform\s+(\w+)\s+(\w+)\s*;', body, re.M):
            if name.startswith('u_bloomUV'):
                calls = ['set2f']
            else:
                calls = re.findall(
                    re.escape(member) + r'\.(set\w+)\("' + re.escape(name) + r'"', cpp)
            for call in calls:
                if call != sizes.get(typ):
                    problems.append(
                        f"{shader}: '{name}' is {typ} but C++ calls {call} "
                        f"(expected {sizes.get(typ)})")


def main():
    check_generated_files_are_current()
    check_setting_defaults()
    check_defaults_match_showcase()
    check_tuner_uses_real_arrow_keys()
    check_preset_buttons_are_listened_for()
    check_shader_uniforms()

    if problems:
        print("FAILED:")
        for p in problems:
            print("  -", p)
        return 1

    mod = json.loads((ROOT / 'mod.json').read_text())
    print(f"ok: {len(mod['settings'])} settings, generated files current, "
          f"shader uniforms consistent")
    return 0


if __name__ == '__main__':
    sys.exit(main())
