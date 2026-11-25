# Euphoria (EPH SAMPLES)

One-knob euphoric lead enhancer. Turn the knob and Ephy the Wizard shows how much you cast.

| Knob | Stage | What happens |
|---|---|---|
| 0-30 % | Charm | width 100 -> 130 %, chorus up to 25 %, tape saturation up to 3 dB |
| 30-70 % | Conjure | hall reverb up to 25 % (decay 1.5 -> 4 s, pre-delay 20 -> 35 ms), tempo-synced ducking up to 6 dB, drive up to 6 dB |
| 70-100 % | Ascend | reverb up to 40 % (6 s), OTT up to 40 %, 10 kHz shelf up to +4 dB (Bright) / +1 dB (Dark), unison doubler +-15 cents, drive 9 dB |

Always: side signal below 150 Hz removed (mono bass), auto gain (level within 0.1 dB over the whole range on a supersaw lead),
20 ms knob smoothing, 10 ms bypass crossfade. Tone switch Dark / Bright (reverb damping 5 / 12 kHz).

## Build

```
cmake -S . -B build -DEUP_BUILD_CHECKS=ON
cmake --build build --config Release --target Euphoria_VST3 Euphoria_Standalone eup_plugin_check
build/eup_plugin_check_artefacts/Release/eup_plugin_check
```

GitHub Actions (`.github/workflows/build.yml`) builds Windows (VST3 + installer) and macOS (VST3 + AU, .pkg) and runs the checks.
Pushes to `main` publish a `test-build` prerelease; `release/x.y.z` branches create a draft release that the owner publishes.

## Licensing

Same license server as HDMM, CEMM and The Final Master (3-day trial, then a serial).
`productId` in `Source/plugin/Licensing.h` is **0** until Euphoria is registered on the server, so activation is refused
("not connected to the store yet") and only the trial works.

## Files

- `Source/dsp/Engine.*` - the audio engine (no JUCE dependency)
- `Source/plugin/Ephy.*` - Ephy the Wizard, drawn as vectors, reacts to the knob and the audio
- `Source/plugin/PluginEditor.*` - knob, Dark/Bright, bypass, stages, readouts, license panel
- `tests/PluginCheck.cpp` - 40 checks: levels, transparency at 0 %, mono bass, bypass, reverb tail, tone, state, editor snapshots
