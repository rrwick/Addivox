# Per-harmonic Settings

In EDIT mode, the main panel contains a row of tabs. Each tab (except for **EQ**) shows a bar chart with one bar per harmonic. You can drag bars up and down to set values, use the **Set shape** dropdown to apply a preset curve, or use the **Run action** dropdown to apply transformations like scaling or normalising. Each action has a keyboard shortcut, shown as a letter in brackets next to the action name. The **X range** controls in the control column let you zoom in on a subset of harmonics. The **All notes** toggle, when enabled, locks that parameter in sync across all key notes. See [Key Notes and Interpolation](key_notes_and_interpolation.md).

For some tabs (**Level**, **Breath**, **Attack** and **Release**), there is a **Macro / Detail** switch on top. When in **Macro** mode, the per-harmonic settings are controlled by a set of knobs, allowing for easy experimentation. When in **Detail** mode, each harmonic can be edited individually for fine control.

### Level

Controls the amplitude of each harmonic at full breath. This is the primary control for shaping the tonal character of a patch — setting the relative strength of each harmonic determines whether the sound is bright or mellow, thin or full.

### Breath

Controls per-harmonic breath sensitivity. At a value of 1, a harmonic's level scales linearly with breath. Higher values make the harmonic require proportionally more breath before it becomes prominent — effectively hiding quieter harmonics until the player blows harder. Lower values make a harmonic appear at low breath levels.

### Attack / Release

Controls how quickly each harmonic can increase (**Attack**) or decrease (**Release**) in level in response to breath changes. Higher attack values make a harmonic bloom more slowly; higher release values make it fade more slowly.

### Pitch

A static pitch offset in cents for each harmonic, added on top of its natural harmonic frequency. Small detuning of individual harmonics can add warmth and movement to the sound.

### Pan

The stereo position of each harmonic, from −1 (fully left) to +1 (fully right). Spreading harmonics across the stereo field can add width and richness to the sound.

### Variation tabs

Six tabs control continuous slow modulation of level, pitch and pan. Each has an **Amount** tab (depth of the modulation for that harmonic) and a **Rate** tab (speed of the modulation). These are scaled globally by the [variation controls](global_settings.md).

- **LvlVarAmt / LvlVarRate** — level variation
- **PchVarAmt / PchVarRate** — pitch variation
- **PanVarAmt / PanVarRate** — pan variation

### Y-axis transformations

Addivox's edit tabs let you view the per-harmonic settings as a bar chart. The y-axis transform controls how a bar's height maps to its actual value.

**Linear** is the most intuitive: a bar twice as tall represents twice the value. But fine adjustments of small values can be difficult.

The **Square root** and **Pseudo-log** transforms both stretch the lower end of the scale, giving you more visual room for small values so adjustments are easier. Square root gives a moderate stretch; Pseudo-log stretches more dramatically.

All of these transform options are purely visual — they do not change the sound produced by Addivox.
