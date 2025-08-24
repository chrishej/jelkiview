# Jelkiview

Jelkiview is a fast, interactive log viewer for timeseries data in CSV format. It is designed for engineers and developers who need to inspect, analyze, and visualize large sets of signal data efficiently. It is a work in progress, and more features will be added. It is based on ImGui and ImPlot, which does the heavy lifting.

## Features

- **Multi-Subplot Layouts:** Organize signals into multiple subplots for clear comparison and analysis.
- **CSV Import:** Load large CSV files containing timeseries data with automatic signal detection. How the CSV file is imported can be customized to support e.g. ";" instead of ",". The name of the time column is by default "Time", but can also be changed.
- **Decimation:** Efficiently handles large datasets by decimating data for smooth plotting and interaction. It never shows more than 10k points for each signal. This makes the performance for long logs with high data rates very good. 
- **Interactive Cursors:** Place vertical cursors on plots to inspect values at specific time points.
- **Custom Layout Save/Load:** Save and load subplot layouts for different analysis scenarios.

## Getting Started
Either download the pre-build binary or build yourself with SCons. In SConstruct change the path to msys2 on your system, or change to MSVC if you prefer it.

## License

See the `LICENSE` file for details.