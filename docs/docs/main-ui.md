# Main UI

The Addivox UI is divided into several areas, described below from top to bottom.

## Patch selector

The top bar shows the name of the currently loaded patch and provides controls to navigate between patches. The left and right arrows step through patches in the current group, and the menu button opens the full patch browser where you can choose any factory or custom patch, save your work, or import and export patch files. See [Patches](patches.md) for more detail.

## VIS / EDIT panel

The large panel in the middle of the UI has two modes, switched with the **VIS / EDIT** toggle in its top-left corner.

**VIS mode** shows a live bar chart of all 100 harmonics as you play. Bar heights update in real time to reflect the current output level of each harmonic, giving you a visual picture of the sound's harmonic content.

**EDIT mode** is where you shape the sound. It contains a set of tabs for editing per-harmonic parameters and the EQ curve:

- **EQ** — draws a frequency-response curve applied to the final sound. See [EQ](eq.md).
- **Level**, **Breath**, **Attack**, **Release**, **Pitch**, **Pan**, and six variation tabs — control the individual behaviour of each of the 100 harmonic oscillators. See [Per-harmonic Settings](per-harmonic-settings.md).

The keyboard at the bottom of the panel selects which key note you are editing in EDIT mode, and plays the corresponding note in VIS mode (see below).

## Settings panels

Below the main panel are five rows of knobs and controls:

- **[Pitch](pitch-settings.md)** — transpose, tuning, portamento.
- **[Envelope](global-settings.md)** — global attack and release scales.
- **[Variation](global-settings.md)** — global scales for level, pitch, and pan variation depth and rate.
- **[Output](global-settings.md)** — overall level, tuning offset, and pan offset.
- **[Effects](effects.md)** — drive, tone, chorus, and reverb.

## Meters

<p align="center"><img src="../assets/meters.png" alt="Addivox meters" style="width: 30%;"></p>

The **breath meter** on the top shows the current incoming breath/CC signal level. The **output meter** on the bottom shows the stereo output level; red indicators at the top warn that the signal is clipping above 0 dB.

## Keyboard

The keyboard along the bottom of the UI highlights the note currently being played. In VIS mode you can click keys to play notes directly (at full breath). In EDIT mode, clicking a key selects which key note's settings are displayed in the editor; if no key note exists at that note, the editor shows the interpolated values for that pitch. See [Key Notes and Interpolation](key-notes-and-interpolation.md).
