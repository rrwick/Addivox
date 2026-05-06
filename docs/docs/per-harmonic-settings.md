# Per-harmonic Settings

In EDIT mode, the main panel contains a row of tabs. Each tab shows a bar chart with one bar per harmonic (up to 100). You can drag bars up and down to set values, use the **Set shape** dropdown to apply a preset curve, or use the **Run action** dropdown to apply transformations like scaling or normalising.

The **X range** controls at the bottom let you zoom in on a subset of harmonics, making it easier to edit the higher harmonics where bars would otherwise be very narrow.

The **All notes** toggle, when enabled, locks that parameter in sync across all key notes. See [Key Notes and Interpolation](key-notes-and-interpolation.md).

## Level

Controls the amplitude of each harmonic at full breath. This is the primary control for shaping the tonal character of a patch — setting the relative strength of each harmonic determines whether the sound is bright or mellow, thin or full.

## Breath

Controls per-harmonic breath sensitivity (the `breath_power` exponent). At a value of 1, a harmonic's level scales linearly with breath. Higher values make the harmonic require proportionally more breath before it becomes prominent — effectively hiding quieter harmonics until the player blows harder. Lower values make a harmonic appear at low breath levels.

## Attack / Release

Controls how quickly each harmonic can increase (**Attack**) or decrease (**Release**) in level in response to breath changes. Higher attack values make a harmonic bloom more slowly; higher release values make it fade more slowly. These are scaled globally by the [envelope controls](global-settings.md).

## Pitch

A static pitch offset in cents for each harmonic, added on top of its natural harmonic frequency. Small detuning of individual harmonics can add warmth and movement to the sound.

## Pan

The stereo position of each harmonic, from −1 (fully left) to +1 (fully right). Spreading harmonics across the stereo field can add width and richness to the sound.

## Variation tabs

Six tabs control continuous slow modulation of level, pitch, and pan. Each has an **Amount** tab (depth of the modulation for that harmonic) and a **Rate** tab (speed of the modulation). These are scaled globally by the [variation controls](global-settings.md).

- **LvlVarAmt / LvlVarRate** — level variation (tremolo-like).
- **PchVarAmt / PchVarRate** — pitch variation (vibrato-like).
- **PanVarAmt / PanVarRate** — pan variation (slow stereo movement).




## Y-axis transformations

Addivox's edit tabs let you view the per-harmonic settings as a bar chart. The y-axis transform controls how a bar's height maps to its actual value.

**Linear** is the most intuitive: a bar twice as tall represents twice the value. But this makes fine adjustments near zero difficult — small values are squashed into a thin sliver at the bottom of the chart.

The **Square root** and **Pseudo-log** transforms both stretch the lower end of the scale, giving you more visual room for near-zero values so small adjustments are easier to make. Square root gives a moderate stretch; Pseudo-log stretches more dramatically.

Pseudo-log is particularly well suited to audio levels. Our ears perceive loudness on a logarithmic scale — this is why volume is measured in decibels. Using Pseudo-log for the "Level" edit tab means equal changes in bar height correspond to roughly equal changes in perceived loudness. (A pure [log transform](https://en.wikipedia.org/wiki/Log_transformation_(statistics)) has one problem: it can never actually reach zero. Pseudo-log is a modified version that behaves like a log for most of its range but allows a bar to be set all the way to zero at the bottom.)

All of these transform options are purely visual — they do not change the sound produced by Addivox.
