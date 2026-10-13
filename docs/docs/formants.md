# Formants

The Formants tab in the editor lets you draw a frequency-response curve that is applied to the synthesised sound. The curve is defined by a set of control points — you can drag existing points to reshape it, and add/remove points by double-clicking. The curve is smoothly interpolated between points using a monotone spline, and it is flat outside the leftmost and rightmost points.

Unlike the per-harmonic settings, the formants curve is anchored to absolute frequencies rather than to the pitch of the note. This means a peak at 800 Hz stays at 800 Hz regardless of whether you are playing a low note or a high note.

<div style="display: flex; justify-content: center; gap: 10px; width: 100%;">
  <img src="../assets/eq_edit.png" alt="Addivox Formants editor" style="width: calc(50% - 5px);">
  <img src="../assets/eq_vis.png" alt="Addivox Formants visualised" style="width: calc(50% - 5px);">
</div>

### What are formants?

A **formant** is a resonant peak in the frequency spectrum — a band of frequencies that the sound emphasises. Formants are what give acoustic instruments and the human voice their characteristic timbre. For example, the difference between a saxophone and an oboe comes in part from where their formants sit in the frequency spectrum.

In real instruments, formants arise from the physical resonances of the instrument. Because these shapes are fixed, the formants stay at roughly the same frequencies even as you play higher or lower notes. Addivox's Formants curve works the same way: keeping the curve fixed in frequency lets you shape formants that behave like those of a real instrument.

Adding one or more peaks can dramatically change the timbre of a patch. The **All notes** toggle (see [Key Notes and Interpolation](key_notes_and_interpolation.md)) is particularly useful here — it lets you set a single curve that applies across all key notes at once, which is usually what you want for formants.

If different key notes have different curves, notes between them blend the two curves as they appear on screen, e.g. halfway between a +12 dB point and a −12 dB point gives 0 dB. A newly added key note starts with a copy of the curve from its nearest key note.
