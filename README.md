# GDDS-Tools

High-performance DirectStorage texture converter for 3DMigoto modding.

Converts standard DirectDraw Surface (`.dds`) textures into GDeflate-compressed DirectStorage (`.gdds`) archives, and decompresses `.gdds` archives back into standard `.dds` textures with lossless SHA-256 byte-level verification.

## Features

- **Toggle Mode**: Drag and drop a mod folder (or loose `.dds` / `.gdds` file) onto `gdds_tool.exe`:
  - If the folder contains `.dds` files $\rightarrow$ compresses them to `.gdds`, rewrites `.ini` texture paths, and removes the originals to save space.
  - If the folder contains `.gdds` files $\rightarrow$ decompresses them back to `.dds`, reverts `.ini` texture paths, and removes the `.gdds` files.
- **Fast Multithreaded Compression**: Parallel compression pipeline utilizing Microsoft DirectStorage GDeflate CPU codecs.
- **GPU Direct Streaming Slices**: Slices large tall atlas textures (`height > 2048` in 2048-pixel steps) into up to 16 independently decompressible DirectStorage chunks for low-latency GPU streaming.
- **Full Lossless Roundtrip**: Reconstructs standard DirectX 10 / legacy DDS headers and matches SHA-256 digests identically.
- **INI Rewrite Engine**: Case-insensitive regex matching for mod configuration files (`.ini`), updating filenames automatically.

---

## Download & Installation

1. Grab the latest release package (`gdds-tool-windows-x64.zip`) from the [Releases](https://github.com/zurce/GDDS-Tools/releases) page.
2. Extract the archive anywhere on your machine.
3. Ensure `dstorage.dll` and `dstoragecore.dll` remain in the same directory as `gdds_tool.exe`.

---

## Usage

### Drag & Drop (Recommended)

Simply drag any mod folder (e.g. `Mods/character/aria/ariakfchonky.zip` or uncompressed directory) directly onto `gdds_tool.exe`.
Dragging the folder again toggles the format back.

### Command Line Interface

```cmd
gdds_tool.exe <path_to_folder_or_file> [options]
```

#### Options:
- `-c, --compress`: Force compression mode (`.dds` $\rightarrow$ `.gdds`).
- `-d, --decompress`: Force decompression mode (`.gdds` $\rightarrow$ `.dds`).
- `-k, --keep`: Preserve original files instead of removing them.
- `--mips <strip|keep>`: Mipmap retention mode.
  - `strip` (**default**): Strips sub-mips to Mip 0 only for maximum compression (~60–75% reduction), matching 3DMigoto DirectStorage mod streaming.
  - `keep` (or `--keep-mips` / `--preserve-mips`): Compresses and preserves all mip levels in the GDDS archive.
- `--strip-mips`: Convenience flag to enforce Mip 0 stripping (maximum compression).
- `--keep-mips`, `--preserve-mips`: Convenience flag to retain full mipmap chains.
- `-h, --help`: Show help instructions.

#### Examples:

```cmd
# Auto-detect and toggle folder (compresses with maximum ratio by default)
gdds_tool.exe "C:\Mods\character\aria"

# Force compression while keeping original DDS files
gdds_tool.exe "C:\Mods\character\aria" -c --keep

# Compress while preserving the complete mipmap pyramid
gdds_tool.exe "C:\Mods\character\aria" --keep-mips

# Decompress a single .gdds texture
gdds_tool.exe "C:\Mods\character\aria\diffuse.gdds" -d
```

---

## Building from Source

### Requirements
- Windows 10/11 x64
- Visual Studio 2022 / Build Tools with MSVC C++20 support
- CMake 3.20+ (optional, build scripts included)

### Build with CMake:
```cmd
cmake -B build -S .
cmake --build build --config Release
```

The compiled binary and necessary runtime libraries (`dstorage.dll`, `dstoragecore.dll`) will be output to `bin/Release/`.

### Build with PowerShell Script:
```powershell
.\scripts\build_windows.ps1
```

Output binary will be located in `bin\gdds_tool.exe`.

---

## License

MIT License. See [LICENSE](LICENSE) for details.
DirectStorage SDK libraries (`dstorage.dll`, `dstoragecore.dll`) are subject to the Microsoft DirectStorage license agreement.
