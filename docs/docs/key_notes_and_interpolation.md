# Key Notes and Interpolation

A **key note** is a MIDI note at which you define an explicit set of per-harmonic values. Addivox uses key notes to let a patch sound different in different registers.

### Single key note

If a patch has only one key note, all played notes share the same per-harmonic settings. The patch sounds the same regardless of octave, except for the fundamental pitch of each harmonic (which always scales with the played note).

### Multiple key notes

If a patch has two or more key notes, Addivox interpolates smoothly between them. When you play a note that falls between two key notes, each per-harmonic value is a weighted blend of the two neighbouring key notes based on how close the played note is to each one. Notes below the lowest key note use the lowest key note's settings; notes above the highest use the highest.

This allows a single patch to have a heavy, lower-harmonic timbre in the bass register and a brighter, thinner timbre in the upper register — matching how real acoustic instruments change character across their range. The factory brass patches use key notes at each octave (C1 through C8) for exactly this reason.

### "All notes" toggle

Each per-harmonic parameter and the EQ curve have an **All notes** toggle in the editor. When enabled, that parameter is kept in sync across every key note: editing it on one key note instantly updates all others. This is useful for settings that should not vary with register — formant EQ curves are the primary example, since real instrument formants stay fixed in frequency regardless of which note is played.