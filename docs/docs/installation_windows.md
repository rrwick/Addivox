# Installation – Windows

Addivox for Windows is delivered as a zip file with a name like `Addivox_v1.0.0_Windows.zip`. After downloading it, double-click to unzip it. You should see several Addivox files:
`Addivox.exe`, `Addivox.clap` and `Addivox.vst3`.

You do not need to install every file. `Addivox.exe` is the standalone version, which runs by itself. The other files are plugin formats, which are loaded inside a DAW such as Ableton Live, Cubase, REAPER or Bitwig Studio.

These instructions also apply to the [Demo version](demo.md) of Addivox. For the Demo version, the filenames use `AddivoxDemo` instead of `Addivox`, for example `AddivoxDemo.exe` and `AddivoxDemo.vst3`.


### Standalone app

`Addivox.exe` is self-contained. Move it anywhere convenient and optionally create a shortcut to it. You can then open Addivox by double-clicking the executable or shortcut.

For live playing, the standalone application may need a low-latency Windows audio driver. ASIO is usually recommended when available. See [Which Windows audio driver type should I use?](faq_and_miscellaneous_tips.md#which-windows-audio-driver-type-should-i-use) for more details.


### Plugins

Copy the plugin files you need to the matching folder below.

| File            | Format | Per-user install location             | All-users install location              | Example DAWs                             |
| --------------- | ------ | ------------------------------------- | --------------------------------------- | ---------------------------------------- |
| `Addivox.vst3`  | VST3   | `%LOCALAPPDATA%\Programs\Common\VST3` | `C:\Program Files\Common Files\VST3`    | Ableton Live, Cubase, REAPER, Studio One |
| `Addivox.clap`  | CLAP   | `%LOCALAPPDATA%\Programs\Common\CLAP` | `C:\Program Files\Common Files\CLAP`    | Bitwig Studio, REAPER                    |


For VST3, copy the complete `Addivox.vst3` directory, not just the file inside it. For CLAP, copy the `Addivox.clap` file.

If you are not sure which plugin format to install, start with `Addivox.vst3` for most modern DAWs. Use `Addivox.clap` if your DAW supports CLAP and you prefer that format.

After copying plugins, quit and reopen your DAW. Some DAWs scan new plugins automatically. Others have a plugin manager or preferences page where you can rescan plugins. If you installed more than one format, your DAW may show Addivox more than once, for example as both a VST3 and a CLAP plugin.


### Windows compatibility

Addivox is built as a 64-bit Windows app/plugin. It is developed and tested on Windows 11 but should also work on Windows 10. Please try the [Demo version](demo.md) before buying Addivox to ensure that it runs on your system.

Very old versions of Windows, 32-bit DAWs and 32-bit plugin formats are not supported.
