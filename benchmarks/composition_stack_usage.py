#!/usr/bin/env python3
"""Compile composition objects for target ABI sizes and individual stack frames.

Uses the actual ESP-IDF logging and endian headers, without linking or flashing.
Frames exclude callees, interrupts, driver work, and arbitrary user sources;
this is not a runtime stack high-water measurement. Run from the repository root.
"""

import argparse
from pathlib import Path
import subprocess


def main():
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path, help="ESP32 target g++ executable")
    parser.add_argument("--idf-include", type=Path, action="append", required=True,
                        help="IDF include directory; repeat for platform headers and sdkconfig")
    parser.add_argument("--roo-root", type=Path, default=repo.parent)
    parser.add_argument("--source-root", type=Path, default=repo)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = args.source_root.resolve()
    compiler = args.compiler.resolve()
    flags = [str(compiler), "-std=gnu++17", "-Os", "-DNDEBUG", "-DESP_PLATFORM",
             "-fno-exceptions", "-fno-rtti", "-fstack-usage", "-ffunction-sections",
             "-fdata-sections", "-ffile-prefix-map=" + str(source) + "=.",
             "-I" + str(source / "src")]
    for directory in args.idf_include:
        flags.append("-I" + str(directory.resolve()))
    for library in ("roo_io", "roo_logging", "roo_time", "roo_flags",
                    "roo_backport", "roo_collections"):
        flags.append("-I" + str(args.roo_root.resolve() / library / "src"))
    units = ("composition/rasterizable_stack", "composition/streamable_stack",
             "core/rasterizable")
    for unit in units:
        subprocess.run(flags + ["-c", str(source / "src/roo_display" / (unit + ".cpp")),
                               "-o", str(output / (Path(unit).name + ".o"))], check=True)
    probe = output / "composition_sizes.cpp"
    probe.write_text("""#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/composition/streamable_stack.h"
extern "C" {
char sizeof_RasterizableStack[sizeof(roo_display::RasterizableStack)];
char sizeof_StreamableStack[sizeof(roo_display::StreamableStack)];
char sizeof_BufferingStream[sizeof(roo_display::internal::BufferingStream)];
}
""")
    subprocess.run(flags + ["-c", str(probe), "-o", str(output / "composition_sizes.o")],
                   check=True)
    nm = compiler.with_name(compiler.name.removesuffix("g++") + "nm")
    sizes = subprocess.check_output([str(nm), "-S", "--size-sort",
                                     str(output / "composition_sizes.o")], text=True)
    print(subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0])
    print("Object sizes (hex byte counts):")
    print(sizes, end="")
    frames = []
    for unit in units:
        usage = output / (Path(unit).name + ".su")
        for line in usage.read_text().splitlines():
            symbol, size, kind = line.rsplit("\t", 2)
            if "roo_display::" in symbol and "_ZTv" not in symbol:
                frames.append((Path(unit).name, symbol, int(size), kind))
    with (output / "frames.tsv").open("w") as report:
        report.write("unit\tsymbol\tframe_bytes\tkind\n")
        for unit, symbol, size, kind in frames:
            report.write(f"{unit}\t{symbol}\t{size}\t{kind}\n")
    print("Individual frame report:", output / "frames.tsv")


if __name__ == "__main__":
    main()
