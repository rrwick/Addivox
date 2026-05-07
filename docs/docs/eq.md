# EQ

The EQ tab in the editor lets you draw a frequency-response curve that is applied to the synthesised sound. The curve is defined by a set of control points — you can add a point by clicking on the curve, drag existing points to reshape it, and remove a point by right-clicking it. The curve is smoothly interpolated between points using a monotone spline, and it is flat (0 dB) outside the leftmost and rightmost points.

Unlike the per-harmonic settings, the EQ curve is anchored to absolute frequencies rather than to the pitch of the note. This means a peak at 800 Hz stays at 800 Hz regardless of whether you are playing a low note or a high note.

*(Screenshot placeholder: the EQ editor with an example formant curve)*

## Formants

A **formant** is a resonant peak in the frequency spectrum — a band of frequencies that the sound emphasises. Formants are what give acoustic instruments and the human voice their characteristic timbre. For example, the difference between a saxophone and a oboe comes in part from where their formants sit in the frequency spectrum.

In real instruments, formants arise from the physical resonances of the instrument. Because these shapes are fixed, the formants stay at roughly the same frequencies even as you play higher or lower notes. Addivox's EQ works the same way: keeping the curve fixed in frequency lets you shape formants that behave like those of a real instrument.

Adding one or more EQ peaks can dramatically change the timbre of a patch. The **All notes** toggle (see [Key Notes and Interpolation](key_notes_and_interpolation.md)) is particularly useful here — it lets you set a single EQ curve that applies across all key notes at once, which is usually what you want for formants.
