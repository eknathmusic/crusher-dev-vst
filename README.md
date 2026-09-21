# CamelClone — JUCE plugin skeleton

## Status
- Distortion, filter, and compressor DSP: ported from the validated Python
  prototypes, cross-checked against their exact measured numbers in
  `test_dsp.cpp` (7/7 checks passing).
- Full plugin: compiles clean and links into a working VST3 + Standalone
  build on Linux (this sandbox has no macOS/Xcode, so that's as far as
  verification could go here) -- one real bug was caught and fixed in the
  process (a `const` member on `CamelStyleCompressor` silently broke
  `std::vector`'s copy-assignment; removed the `const`, no behavior change).
- Not yet done: custom GUI (using JUCE's auto-generated generic editor for
  now, per the "DSP first" plan), parameter smoothing (params update once
  per audio block, not per-sample -- fine for now, will zipper-click under
  fast automation), AU target (needs building on macOS itself).

## Building on your Mac (Apple Silicon)

1. Install Xcode (from the App Store) and CMake (`brew install cmake`).
2. In this folder, clone JUCE next to `CMakeLists.txt`:
   ```
   git clone --depth 1 https://github.com/juce-framework/JUCE.git
   ```
3. Configure + build:
   ```
   cmake -B build -G Xcode
   cmake --build build --config Release
   ```
   (`-G Xcode` gives you an .xcodeproj too, if you'd rather work in the IDE
   than the command line from here on.)
4. Output lands in `build/CamelClone_artefacts/Release/`:
   - `VST3/CamelClone.vst3` — copy to `~/Library/Audio/Plug-Ins/VST3/`
   - `AU/CamelClone.component` — copy to `~/Library/Audio/Plug-Ins/Components/`
   - `Standalone/CamelClone.app` — runs on its own, no DAW needed, good for
     quick listening tests

First launch of a new AU, macOS/your DAW needs to validate it -- if it
doesn't show up right away, restart the DAW or run
`auval -v aufx Ccl1 Ycmp` in Terminal to force (re-)validation and see any
errors directly.

## Building without installing anything locally (GitHub Actions)

`.github/workflows/build.yml` in this project builds the plugin on a real
Apple Silicon macOS machine in GitHub's cloud -- no Xcode, Homebrew, or
CMake on your own machine at all.

1. Create a GitHub account if you don't have one (free).
2. Create a new repository and push this folder to it. **Make it public**
   if you want builds to be completely free with no minute limits (private
   repos still work using your account's free included minutes, but macOS
   jobs consume those at 10x the wall-clock time, so they run out fast).
3. The workflow runs automatically on every push to `main`/`master`, or
   trigger it manually from the repo's Actions tab (`Run workflow`).
4. When the run finishes (usually a few minutes), open the run, scroll to
   **Artifacts**, and download `CamelClone-macOS-arm64.zip` -- it contains
   the `.vst3`, `.component` (AU), and Standalone `.app`, all built for
   Apple Silicon, ready to copy into your plugin folders as described above.

## Source layout

```
Source/
  DSP/
    Distortion.h      Tube + Mech waveshaping (pure C++, no JUCE dependency)
    ResonantFilter.h  TPT State Variable Filter (cutoff + resonance)
    Compressor.h       Single-knob Amount + Phat, causal auto-makeup gain
  PluginProcessor.h/.cpp   Wires the three DSP classes into processBlock,
                           APVTS parameters, master safety limiter
test_dsp.cpp           Standalone test (g++ test_dsp.cpp -> ./a.out), no
                        JUCE needed -- rerun this after touching DSP/ code
```

## If you change the DSP

Run `test_dsp.cpp` first (seconds, no JUCE rebuild needed) to catch math
bugs before waiting on a full plugin recompile:
```
g++ -std=c++17 -O2 -Wall -Wextra -o test_dsp test_dsp.cpp && ./test_dsp
```
