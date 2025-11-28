# trdpTool

A minimal TRDP-oriented CMake project that scaffolds a simple simulator application. The repository contains:

- **trdp-core** – core C++ abstractions for datasets, PD/MD templates, and a thin TRDP session wrapper.
- **apps/trdp-sim** – an interactive console that exercises the core logic with a stub configuration.

## Building

1. Configure with CMake and optionally point to an existing TRDP build:

```bash
cmake -S . -B build \
  -DTRDP_INCLUDE_DIR=/path/to/trdp/includes \
  -DTRDP_LIB_PATH=/path/to/libtrdp.a
cmake --build build
```

If `TRDP_LIB_PATH` is omitted, the project builds in a stub mode so you can explore the CLI without the TRDP stack installed.

## Running the simulator

```bash
./build/apps/trdp-sim/trdp-sim
```

Type `help` at the prompt for available commands (PD/MD listing, setting element values, and sending an MD template). The default configuration ships with a small example dataset to make the tool usable even before wiring real XML files.

## Next steps

- Replace the stub XML loader in `trdp-core/src/config.cpp` with TAU XML parsing to populate datasets and telegrams.
- Hook PD publish/subscribe and MD request/reply flows into the TRDP C API by defining `TRDP_AVAILABLE` via `TRDP_LIB_PATH`.
- Add HTTP endpoints if you need remote control for automated tests or lightweight dashboards.

