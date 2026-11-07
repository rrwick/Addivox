# Key Notes and Interpolation

A **key note** is a MIDI note at which you define an explicit set of per-harmonic values. Addivox uses key notes to let a patch sound different in different registers.

### Single key note

<p align="center"><img src="../assets/key_note_single.png" alt="Addivox single key note" style="max-width: 100%;"></p>

If a patch has only one key note, all played notes share the same per-harmonic settings. Formants and Resonance can still change the timbre across registers as harmonics move through their frequency-based curves.


### Multiple key notes

<p align="center"><img src="../assets/key_note_multiple.png" alt="Addivox multiple key note" style="max-width: 100%;"></p>

If a patch has two or more key notes, Addivox interpolates smoothly between them. This allows a patch's timbre to differ over its range. When you play a note that falls between two key notes, each per-harmonic value is a weighted blend of the two neighbouring key notes based on how close the played note is to each one. Notes below the lowest key note use the lowest key note's settings; notes above the highest use the highest.


### Formants and Resonance curves

Formants and Resonance curves are blended in one of two ways, depending on whether the curves have the same number of points:

- **Same number of points:** each point glides from its place on one key note to its place on the other, so peaks and dips can move across the spectrum. Points are paired in frequency order, e.g. the lowest point on one key note pairs with the lowest on the other.
- **Different numbers of points:** one curve fades into the other, so peaks and dips will not move across the spectrum.


### "All notes" checkbox

Each edit tab has an **All notes** checkbox in the editor. When enabled, that parameter is kept in sync across every key note: editing it on one key note instantly updates all others. This is useful for settings that do not need to vary with register.
