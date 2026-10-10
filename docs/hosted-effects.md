# Hosted AU and VST3 effects (test build)

This fork adds an **Effect** block to the TONE3000 signal chain on macOS.
It hosts installed Audio Unit and VST3 audio effects with JUCE. Instruments,
VST2 plug-ins, AUv3 extensions, and MIDI effects are outside this test build.

1. Open the standalone app and right-click an empty `+` chain tile.
2. Choose **Add AU/VST3 Effect** — an in-app browser, not a Finder dialog. The
   left column lists **vendors**; picking one opens that vendor's effects in the
   right column and **blurs the vendor column** behind it (backdrop blur). Pick an
   effect to load it. Vendors come from each bundle's reverse-DNS identifier
   (e.g. `com.fabfilter…` → *FabFilter*); the same name in a vendor is shown as
   `(AU)` / `(VST3)`. The list is built from a directory listing of
   `/Library/Audio/Plug-Ins/Components` and `/Library/Audio/Plug-Ins/VST3` — no
   scanning or instantiation, so it opens instantly.
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
