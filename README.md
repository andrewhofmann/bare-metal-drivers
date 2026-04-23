# bare-metal-drivers

Register-level peripheral drivers for STM32 — no HAL dependency

## Prerequisites

- `arm-none-eabi-gcc` toolchain
- CMake 3.22+
- OpenOCD (for flashing via ST-Link)

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The build produces `firmware.elf`, `.bin`, and `.hex` in the `build/` directory.

To flash to an STM32F103 (Blue Pill) board:

```sh
cmake --build build --target flash
```

## Status

Under active development.

## License

MIT
