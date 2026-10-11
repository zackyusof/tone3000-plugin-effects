# Fork distribution identity — THIS FILE EXISTS ONLY IN THE FORK
# (zackyusof/tone3000-plugin-effects). Upstream has no such file, so the
# OPTIONAL include in plugin/CMakeLists.txt is a no-op there and every
# default below (the upstream values) applies.
#
# WHY THIS EXISTS
# Audio plugin formats key installed plugins by manufacturer + type codes
# (AU: aumf/T3K0/TFTF), UIDs derived from them (VST3/AAX), permanent URIs
# (LV2) and IDs (CLAP), plus bundle file names (TONE3000.component/.vst3).
# Binaries built with the upstream values overwrite / collide with official
# TONE3000 releases in a DAW scan. So this fork has two build tracks:
#
#   1. DEFAULT (no flag): upstream identities. Use for local-only builds and
#      for any pull request back to tone-3000/tone3000-plugin — its PR
#      template pins LV2 URI, CLAP ID, AU parameter hints and state format
#      as compatibility contracts. Keep them identical there.
#   2. PUBLIC RELEASES: configure with -DT3K_FORK_NAMESPACED=ON. Ships under
#      the fork identities below, which coexist with official installs.
#      Our GitHub release workflow and build_tone3000.py --namespaced use it.
#
# STABILITY: never change the fork values once released — hosts key existing
# sessions and presets by them, exactly like the upstream rule. The DAW state
# tag ("TONE3000State") is deliberately NOT renamed so presets saved by
# earlier fork builds keep loading. Factory-preset read paths (official
# installer locations) are also left alone as a fallback.
#
# NOTE for iOS builds: this layer does not touch T3K_IOS_BUNDLE_ID (macOS-only
# distribution). Pass -DT3K_IOS_BUNDLE_ID=<your id> as before if needed.

option(T3K_FORK_NAMESPACED
  "Build with fork distribution identities (public releases only)" OFF)

set(T3K_FORK_DISPLAY_NAME "TONE3000 FX"
  CACHE STRING "Fork plugin display name (host menus, window titles)")
set(T3K_FORK_PRODUCT_NAME "TONE3000-FX"
  CACHE STRING "Fork bundle/file name (no spaces: .component/.vst3/.app)")
set(T3K_FORK_PLUGIN_CODE "T3KX"
  CACHE STRING "Fork plugin type code, 4 chars (AU type + VST3/AAX UID seed)")
set(T3K_FORK_MANUFACTURER_CODE "ZYUS"
  CACHE STRING "Fork manufacturer code, 4 chars")
set(T3K_FORK_COMPANY_NAME "ZYUSOF"
  CACHE STRING "Fork vendor name (host menus, bundle IDs)")
set(T3K_FORK_LV2_URI "https://www.zyusof.net/tone3000-fx"
  CACHE STRING "Fork LV2 URI (permanent once released)")
set(T3K_FORK_CLAP_ID "net.zyusof.tone3000-fx"
  CACHE STRING "Fork CLAP ID (permanent once released)")
set(T3K_FORK_DATA_DIR "TONE3000-FX"
  CACHE STRING "Fork app-data folder name (presets, settings, logs, models)")

if(T3K_FORK_NAMESPACED)
  set(PLUGIN_NAME "${T3K_FORK_DISPLAY_NAME}")
  set(PLUGIN_CODE "${T3K_FORK_PLUGIN_CODE}")
  set(T3K_DISPLAY_NAME "${T3K_FORK_DISPLAY_NAME}")
  set(T3K_PRODUCT_NAME "${T3K_FORK_PRODUCT_NAME}")
  set(T3K_COMPANY_NAME "${T3K_FORK_COMPANY_NAME}")
  set(T3K_MANUFACTURER_CODE "${T3K_FORK_MANUFACTURER_CODE}")
  set(T3K_LV2_URI "${T3K_FORK_LV2_URI}")
  set(T3K_CLAP_ID "${T3K_FORK_CLAP_ID}")
  set(T3K_APP_DATA_DIR_NAME "${T3K_FORK_DATA_DIR}")
endif()
