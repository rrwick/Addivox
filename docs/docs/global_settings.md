# Global settings

## Envelope

<p align="center"><img src="../assets/envelope.png" alt="Addivox envelope settings" class="float-right" style="max-width: 50%; width: 117px;"></p>

**Attack** and **Release** are scale multipliers applied on top of each harmonic's individual attack and release times (set in the [per-harmonic editor](per-harmonic_settings.md)).

- **Attack** — higher values slow down how quickly harmonics bloom in when a note starts.
- **Release** — higher values let harmonics ring out longer after a note ends.


## Variation

<p align="center"><img src="../assets/variation.png" alt="Addivox variation settings" class="float-right" style="max-width: 50%; width: 259px;"></p>

Variation adds continuous slow modulation to the sound, giving it movement and life. Each of the three modulation targets — **level**, **pitch**, and **pan** — has two controls:

- **Amount** — scales the depth of that modulation across all harmonics.
- **Rate** — scales the speed of that modulation across all harmonics.

The per-harmonic variation depths and rates (set in the [per-harmonic editor](per-harmonic_settings.md)) are multiplied by these global scales, so setting an Amount to 0 silences that variation entirely. Level variation gives a subtle tremolo-like effect; pitch variation adds vibrato; pan variation slowly moves harmonics left and right in the stereo field.


## Output

<p align="center"><img src="../assets/output.png" alt="Addivox output settings" class="float-right" style="max-width: 50%; width: 118px;"></p>

- **Level** — sets the overall output volume of the synth.
- **Tuning** — applies a global pitch offset in cents to all harmonics. Unlike most controls, this is not tied to the current patch and will keep its value when you switch patches.
- **Pan** — shifts all harmonics left or right in the stereo field. Also not patch-tied.
