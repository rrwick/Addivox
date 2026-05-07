# Pitch settings

<p align="center"><img src="../assets/pitch.png" alt="Addivox pitch settings" class="float-right" style="max-width: 50%; width: 197px;"></p>

Addivox is a **monosynth** — it plays one note at a time.

**Transpose** shifts every played note by a fixed number of semitones. The keyboard display still highlights the incoming MIDI note, but the sounded pitch is shifted by the transpose amount. This control is not patch-tied and keeps its value when you switch patches.

**Tuning** applies a global pitch offset in cents (hundredths of a semitone) to all harmonics. Like transpose, this is not patch-tied.

**Portamento** controls smooth pitch glides between notes. The rate of the glide is set by MIDI CC5 (typically the breath controller's bite sensor on a wind controller). The **Min** and **Max** values set the glide time range: **Min** is the glide time when CC5 is 0 (usually instant or very fast), and **Max** is the glide time when CC5 is at full value.
