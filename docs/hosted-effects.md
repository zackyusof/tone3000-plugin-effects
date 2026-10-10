# Hosted AU and VST3 effects (test build)

This fork adds an **Effect** block to the TONE3000 signal chain on macOS.
It hosts installed Audio Unit and VST3 audio effects with JUCE. Instruments,
VST2 plug-ins, AUv3 extensions, and MIDI effects are outside this test build.

1. Open the standalone app and right-click an empty `+` chain tile.
2. Choose **Add AU/VST3 Effect** — a menu listing every installed Audio Unit and
   VST3 effect, grouped first by format (**Audio Units** / **VST3**) then by
   first letter. It is built from a plain directory listing of the standard
   system locations (`/Library/Audio/Plug-Ins/Components` for Audio Units,
   `/Library/Audio/Plug-Ins/VST3` for VST3) — no scanning or instantiation, so it
   opens instantly. With large collections (1000+ effects) the letter submenus
   are what keep it navigable.
3. Click the effect tile to open its own editor. The tile can be powered off,
   removed, dragged to another position, duplicated, and saved in presets.

The hosted plug-in must still be installed when a preset or DAW project is
reopened. If it is absent, the block remains visible and passes audio through.
Its saved state is retained so the effect can load when the plug-in is restored.

This is an in-process host. A faulty third-party effect can crash TONE3000 or
the DAW running it. Start testing in the standalone app with a simple AU/VST3
effect. Serial effect latency is reported to the outer host; parallel branch
alignment and latency changes made inside an open effect editor still need
dedicated validation.
