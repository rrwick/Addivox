# Pitch settings

<p align="center"><img src="../assets/pitch.png" alt="Addivox pitch settings" class="float-right" style="max-width: 50%; width: 197px;"></p>

**Transpose** shifts every played note by a fixed number of semitones. The keyboard display still highlights the incoming MIDI note, but the sounded pitch is shifted by the transpose amount. This control is not patch-tied and keeps its value when you switch patches.

**Tuning** applies a global pitch offset in cents (hundredths of a semitone) to all harmonics. Like transpose, this is not patch-tied.

**Portamento** controls smooth pitch glides between notes. **Min** and **Max** set the glide time range. In Settings, **Portamento CC** selects CC 5 (continuous min–max) or CC 65 (binary min or max). Portamento time is in seconds/semitone, so the glide rate is constant and larger intervals take longer to complete. If the previous note has completely stopped (no harmonics are still releasing), the next note starts on pitch with no glide. If harmonics are still releasing, the glide is shortened by the length of the gap between the notes, so after a long enough gap the next note also starts on pitch.
