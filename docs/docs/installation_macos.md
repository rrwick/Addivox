# Installation – macOS

Addivox for macOS is delivered as a zip file with a name like `Addivox_v1.0.0_macOS.zip`. After downloading it, double-click to unzip it. You should see several Addivox files:
`Addivox.app`, `Addivox.component`, `Addivox.clap`, `Addivox.vst` and `Addivox.vst3`.

You do not need to install every file. `Addivox.app` is the standalone version, which runs by itself and also contains the Audio Unit v3 plugin. The other files are additional plugin formats, which are loaded inside a DAW such as GarageBand, Logic Pro or Ableton Live.

These instructions also apply to the [Demo version](demo.md) of Addivox. For the Demo version, the filenames use `AddivoxDemo` instead of `Addivox`, for example `AddivoxDemo.app` and `AddivoxDemo.vst3`.


### Standalone app

Copy `Addivox.app` to your Mac's main `/Applications` folder. In Finder, this is usually shown as **Applications** in the sidebar. macOS may ask for an administrator password when you copy the app there. You can then open Addivox from Launchpad, Spotlight or Finder.

If macOS shows a warning the first time you open Addivox, try right-clicking `Addivox.app` and choosing **Open**. macOS may then ask you to confirm that you want to open it.


### Audio Unit v3

The Audio Unit v3 version of Addivox is inside `Addivox.app`. This is normal for AUv3 plugins on macOS: the app acts as a container for a small app extension, and macOS makes that extension available to compatible DAWs.

To install the AUv3 version, copy `Addivox.app` to the main `/Applications` folder. This is the system-wide Applications folder, not a folder named `Applications` inside your home folder. There is no separate AUv3 file to copy into `Library/Audio/Plug-Ins`. After installing the app, quit and reopen your DAW. If Addivox does not appear, open `Addivox.app` once, then quit and reopen your DAW again.

Do not remove `Addivox.app` after the AUv3 plugin appears in your DAW. If you move the app to a different location or delete it, macOS may no longer be able to find the AUv3 plugin.


### Plugins

Copy the plugin files you need to the matching folder below. The `~` symbol means your home folder.

| File                  | Format        | Install location                                    | Example DAWs                                |
| --------------------- | ------------- | --------------------------------------------------- | ------------------------------------------- |
| `Addivox.app`         | Audio Unit v3 | `/Applications`                                     | Logic Pro, GarageBand, MainStage            |
| `Addivox.component`   | Audio Unit v2 | `~/Library/Audio/Plug-Ins/Components/`              | Logic Pro, GarageBand, Ableton Live, REAPER |
| `Addivox.vst3`        | VST3          | `~/Library/Audio/Plug-Ins/VST3/`                    | Ableton Live, Cubase, REAPER, Studio One    |
| `Addivox.vst`         | VST2          | `~/Library/Audio/Plug-Ins/VST/`                     | Ableton Live, REAPER                        |
| `Addivox.clap`        | CLAP          | `~/Library/Audio/Plug-Ins/CLAP/`                    | Bitwig Studio, REAPER                       |

If you are not sure which plugin format to install, start with `Addivox.app` for Logic Pro, GarageBand or MainStage, and `Addivox.vst3` for most other modern DAWs. `Addivox.component` and `Addivox.vst` are mainly useful for older DAWs.

The `Library` folder inside your home folder is hidden by default. To open one of these folders in Finder:

1. Open Finder.
2. Choose **Go** > **Go to Folder...** from the menu bar.
3. Paste the install location from the table above.
4. Press Return.
5. Create the folder if it does not already exist, then copy the Addivox plugin file into it.

After copying plugins, quit and reopen your DAW. Some DAWs scan new plugins automatically. Others have a plugin manager or preferences page where you can rescan plugins. If you installed more than one format, your DAW may show Addivox more than once, for example as both an Audio Unit and a VST3.


### Intel and Apple Silicon Macs

Addivox is built as a 64-bit macOS app/plugin and is intended to work on both older Intel Macs and newer Apple Silicon Macs. There is no separate Intel download or Apple Silicon download; use the same files from `Addivox_v1.0.0_macOS.zip`.

All of the included formats can be used on Intel or Apple Silicon Macs, provided your DAW supports that plugin format:

- `Addivox.app`
- `Addivox.component`
- `Addivox.vst3`
- `Addivox.vst`
- `Addivox.clap`

On an Intel Mac, use Addivox normally. On an Apple Silicon Mac, Addivox can run natively in Apple Silicon DAWs. It can also be used from Intel-only DAWs running under Rosetta, provided the DAW supports the plugin format you installed.

Addivox requires macOS 10.13 High Sierra or newer. Very old 32-bit DAWs and 32-bit plugin formats are not supported.
