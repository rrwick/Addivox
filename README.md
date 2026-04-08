# Addivox

Created by running iPlug2's `duplicate.py` on the [IPlugInstrument example](https://github.com/iPlug2/iPlug2/tree/master/Examples/IPlugInstrument).

This is an out-of-source build that requires iPlug2 as a submodule.

Also requires iPlug2 dependencies/SDKs to be downloaded in the iPlug2 repo (see iPlug2 `Dependencies/` scripts).




## CLI tool

Build instructions:
```bash
rm -rf build
cmake -S Addivox -B build -DIPLUG2_DIR="$PWD/iPlug2" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target addivox-cli --parallel 4
```

Example commands:
```bash
preset_dir=/path/to/Addivox/Addivox/presets

addivox -p "$preset_dir"/01_brass.toml --note 60 --seconds 5 --breath 32 -o brass_C4_pp.wav
addivox -p "$preset_dir"/01_brass.toml --note 60 --seconds 5 --breath 64 -o brass_C4_mf.wav
addivox -p "$preset_dir"/01_brass.toml --note 60 --seconds 5 --breath 96 -o brass_C4_ff.wav
```