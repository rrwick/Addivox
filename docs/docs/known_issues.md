# Known issues

## iOS custom patch saving

In the iOS standalone app, the correct place to save custom patches is `On My iPad/Addivox/Addivox Patches/`. The save dialog may not always open there automatically, so you may need to navigate to that folder yourself. Patches saved elsewhere are still exported, but they will not appear under **User Patches** unless they are placed in `Addivox Patches/`. This limitation comes from the iOS file picker. Addivox can export a patch file, but iOS does not give the app the same Finder-style control over the initial save location that macOS and Windows provide.


## iOS AUv3 custom patches

The iOS AUv3 plugin can load custom patches saved by the standalone app, but it cannot save or import custom patches itself. Use the standalone app to create, save and import custom patches, then use **Refresh** in the plugin if needed. This is because iOS app extensions run in a stricter sandbox than the standalone app. The AUv3 plugin cannot directly manage the user-visible `On My iPad/Addivox/Addivox Patches/` folder, so the standalone app mirrors that folder to a private shared location that the plugin can read.
