# Gocator

GoPxL SDK based Gocator control side project.

## Goal

Keep the public Gocator facade, low-level SDK helpers, and Qt widgets separated in the same outer shape as the Camera side project.

## Structure

- `GoPxL-SDK/`: local SDK copy.
- `C++/Gocator.*`: public GoPxL system endpoint facade used by GraphicsEngine.
- `C++/GocatorTypes.h`: neutral connection constants and config.
- `C++/Internal/GocatorConnection.*`: SDK runtime, manual IP connection, and lifecycle wrapper.
- `C++/Internal/GocatorDiscovery.*`: device discovery and manual target config.
- `C++/Internal/GocatorAcquisition.*`: GDP single-frame acquisition wrapper.
- `C++/Internal/GocatorSettingsManager.*`: REST read/update/call and scan output setup.
- `C++/Internal/GocatorResourceClient.*`: REST resource access wrapper.
- `C++/Utility/Qt/QGocatorWidget.*`: Qt control widget aligned with Camera widget metrics.
- `src/main.cpp`: combined CLI/UI entry point.
- `src/cli_main.cpp`: CLI logic.
- `src/ui_main.cpp`: Qt Widgets debug UI logic.

## Build

```bash
mkdir build && cd build
cmake ..
make -j4
```

The Qt debug UI is built when Qt6 Widgets is available.

## Run

```bash
# GUI, no arguments
./build/gocator

# force GUI
./build/gocator --ui

# CLI
./build/gocator discover
./build/gocator <sensor-ip> info
./build/gocator <sensor-ip> read /scan/visibleSensors/
```

## Image Debugging

- If images do not appear after connection:
  1. Use **List Sources** to inspect supported output sources.
  2. Use **Surface Output** to enable Surface output.
  3. Use **Set Output** to request a source such as `topIntensityImage`.
  4. 16-bit payloads such as surface heightmaps are normalized for preview.
  5. The tree prefers schema `label` or `title` for display names.
  6. Refresh tries standard discovery first, then classic discovery for older G2/G3 sensors.

No-argument execution is the CLion smoke path: it tries SDK discovery first, then falls back to `192.168.1.10 info`.

`profile-output` stops the device when possible, then configures profile mode, Gocator Protocol, and one GDP output.

The debug UI can connect, inspect scanner info, tune scan mode/intensity/uniform spacing/optional exposure, configure one GDP output source, and grab frames until an image or valid profile is received. Preview is available for common grayscale/RGB image payloads and profile payloads.
