# Installation – Windows

The Windows archive contains the standalone application, VST3 plugin and CLAP plugin. Factory patches and interface resources are embedded in the binaries.

## Standalone application

`Addivox.exe` (or `AddivoxDemo.exe`) is self-contained. Move it anywhere convenient and optionally create a shortcut to it.

## VST3 plugin

Copy the complete `Addivox.vst3` (or `AddivoxDemo.vst3`) directory into either location:

- Per-user: `%LOCALAPPDATA%\Programs\Common\VST3`
- All users: `C:\Program Files\Common Files\VST3`

Restart your DAW or ask it to rescan VST3 plugins.

## CLAP plugin

Copy `Addivox.clap` (or `AddivoxDemo.clap`) into:

`%LOCALAPPDATA%\Programs\Common\CLAP`

Restart your DAW or ask it to rescan CLAP plugins.
