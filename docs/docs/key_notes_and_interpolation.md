# Key Notes and Interpolation

A **key note** is a MIDI note at which you define an explicit set of per-harmonic values. Addivox uses key notes to let a patch sound different in different registers.

### Single key note

If a patch has only one key note, all played notes share the same per-harmonic settings. The patch sounds the same regardless of octave, except for the fundamental pitch of each harmonic (which always scales with the played note).

### Multiple key notes

If a patch has two or more key notes, Addivox interpolates smoothly between them. When you play a note that falls between two key notes, each per-harmonic value is a weighted blend of the two neighbouring key notes based on how close the played note is to each one. Notes below the lowest key note use the lowest key note's settings; notes above the highest use the highest.

This allows a single patch to have a heavy, lower-harmonic timbre in the bass register and a brighter, thinner timbre in the upper register — matching how real acoustic instruments change character across their range. The factory brass patches use key notes at each octave (C1 through C8) for exactly this reason.

### Formants and Resonance curves

Formants and Resonance curves are blended in one of two ways, depending on whether the two key notes' curves have the same number of points:

- **Same number of points:** each point glides from its place on one key note to its place on the other, so peaks and dips move across the spectrum. Points are paired in frequency order: the lowest point on one key note pairs with the lowest on the other, and so on. For example, halfway between a +12 dB peak at 500 Hz and a +12 dB peak at 2 kHz, there is a +12 dB peak at 1 kHz.
- **Different numbers of points:** one curve fades into the other as they appear on screen. Halfway between the same two peaks, there are two +6 dB peaks, at 500 Hz and 2 kHz.

To make curves with the same number of points fade rather than glide, add a point to one of them where it doesn't change the shape, such as on a flat stretch.

When you add a key note between two others whose curves have the same number of points, it starts with the blend of their curves, so the sound doesn't change. Otherwise, it copies the curves from the nearer key note.

### "All notes" toggle

Each per-harmonic parameter, the formants curve and the resonance setup have an **All notes** toggle in the editor. When enabled, that parameter is kept in sync across every key note: editing it on one key note instantly updates all others. This is useful for settings that should not vary with register — formants curves are the primary example, since real instrument formants stay fixed in frequency regardless of which note is played.