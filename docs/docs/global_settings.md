# Global settings

### Envelope

<p align="center"><img src="../assets/envelope.png" alt="Addivox envelope settings" class="float-right" style="max-width: 50%; width: 117px;"></p>

**Attack** and **Release** are scale multipliers applied on top of each harmonic's individual attack and release times (set in the [per-harmonic editor](per-harmonic_settings.md)).

- **Attack** — higher values slow down how quickly harmonics bloom when a note starts.
- **Release** — higher values let harmonics ring out longer after a note ends.


### Variation

<p align="center"><img src="../assets/variation.png" alt="Addivox variation settings" class="float-right" style="max-width: 50%; width: 259px;"></p>

Variation adds continuous modulation to each harmonic, giving the sound movement. Each of the three modulation targets (**level**, **pitch** and **pan**) has two controls:

- **Amount** — scales the depth of that modulation across all harmonics.
- **Rate** — scales the speed of that modulation across all harmonics.

The per-harmonic variation depths and rates (set in the [per-harmonic editor](per-harmonic_settings.md)) are multiplied by these global scales, so setting an Amount to 0 silences that variation entirely.


### EQ

<p align="center"><img src="../assets/eq.png" alt="Addivox EQ settings" class="float-right" style="max-width: 50%; width: 118px;"></p>

These knobs scale the strength of the EQ curves: 0× is off, 1× applies the curve as drawn, and higher values (up to 3×) exaggerate its boosts and cuts.

- **Formants** — scales the fixed-frequency [Formants curve](formants.md).
- **Resonance** — scales the breath-controlled [Resonance curve](resonance.md).


### Output

<p align="center"><img src="../assets/output.png" alt="Addivox output settings" class="float-right" style="max-width: 50%; width: 118px;"></p>

- **Pan** — shifts all harmonics left or right in the stereo field. This is not tied to the current patch and will keep its value when you switch patches.
- **Level** — sets the overall output volume of the synth. If the output meter is showing red bars, lower the value of this knob.
