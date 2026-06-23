# Installation – Windows

The Windows archive contains the standalone application, VST3 plugin, CLAP plugin and factory patches.

## Standalone application

Keep `Addivox.exe` (or `AddivoxDemo.exe`) and the `factory_patches` directory together. You may move that directory anywhere convenient and create a shortcut to the executable.

## VST3 plugin

Copy the complete `Addivox.vst3` (or `AddivoxDemo.vst3`) directory into either location:

- Per-user: `%LOCALAPPDATA%\Programs\Common\VST3`
- All users: `C:\Program Files\Common Files\VST3`

Restart your DAW or ask it to rescan VST3 plugins.

## CLAP plugin

Copy `Addivox.clap` (or `AddivoxDemo.clap`) and the `factory_patches` directory into:

`%LOCALAPPDATA%\Programs\Common\CLAP`

Keep the CLAP plugin and `factory_patches` directory together. Restart your DAW or ask it to rescan CLAP plugins.
