# RF Logger

Cross-platform RF spectrum analyzer for the pro audio industry. Capture, visualize, and export RF spectrum data for use with **Shure Wireless Workbench** frequency coordination.

## Features

- **Real-time spectrum display** with live, max hold, and average traces
- **Waterfall / spectrogram** view for signal activity over time
- **Frequency markers** for quick identification of key frequencies
- **Shure WWB export** — direct CSV export compatible with Wireless Workbench import
- **Generic CSV export** with metadata headers for general analysis
- **Dark theme** optimized for stage and production environments

## Supported Devices

| Device | Connection | Status |
|--------|-----------|--------|
| RF Explorer | USB Serial (500000 baud) | ✅ Full support |
| TinySA Basic/Ultra | USB Serial (115200 baud) | ✅ Full support |
| RTL-SDR | USB (librtlsdr) | 🔧 Optional, requires librtlsdr |

## Building

### Prerequisites

- **CMake** 3.21+
- **Qt 6.6+** with modules: Widgets, Core, Gui, SerialPort, PrintSupport
- **C++20** compiler (MSVC 2022, GCC 12+, Clang 14+, Apple Clang 15+)
- **librtlsdr** (optional, for RTL-SDR device support)

### Build Steps

```bash
# Clone
git clone https://github.com/jpwalters/RFLogger.git
cd RFLogger

# Configure (QCustomPlot and KissFFT are fetched automatically)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release --parallel

# Run tests
cd build && ctest --output-on-failure -C Release
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `RFLOGGER_BUILD_TESTS` | ON | Build unit tests |
| `RFLOGGER_ENABLE_RTLSDR` | ON | Enable RTL-SDR support (requires librtlsdr) |
| `RFLOGGER_VERSION` | from project() | Override version from CI tag |

## Usage

1. **Connect** your RF Explorer or TinySA via USB
2. **Select** the serial port and device type (or use Auto Detect)
3. **Set** the frequency range and sweep points
4. **Start scanning** to see live spectrum data
5. **Export** to Shure WWB CSV format for frequency coordination

### WWB Export Format

The exported CSV follows Shure's expected format — two columns, no header:
```
470.000000,-85.5
470.500000,-72.3
471.000000,-100.0
```

Column 1: frequency in MHz (6 decimal places)
Column 2: amplitude in dBm (1 decimal place)

## Releases

Pre-built binaries are available on the [Releases](https://github.com/jpwalters/RFLogger/releases) page for Windows, macOS (Apple Silicon), and Linux. Tagged pushes (`v*`) trigger automated builds via GitHub Actions.

## License

[MIT](LICENSE) — Copyright 2026 James P. Walters
