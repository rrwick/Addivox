# Effects

<p align="center"><img src="../assets/effects.png" alt="Addivox effects" class="float-right" style="max-width: 50%; width: 187px;"></p>

Addivox includes a small set of effects for shaping the final sound:

- **Drive** is a saturation/distortion effect. It blends in a tanh-style waveshaper, with more gain, bias and smoothing as the control increases. Low settings add warmth, and high settings become more distorted.
- **Tone** is a tilt EQ. Negative values make the sound darker by emphasising lower frequencies, while positive values make it brighter by emphasising higher frequencies.
- **Chorus** is an 8-voice stereo chorus to widen and thicken the sound.
- **Reverb** adds synthetic room ambience. Low settings are shorter and early-reflection focused. High settings add longer pre-delay and a longer tail.

Unlike the rest of Addivox, which operates on a per-harmonic oscillator basis, these effects are applied to the final stereo waveform after synthesis. They are applied in the order listed above: drive, tone, chorus, reverb. Using effects (any setting other than 0) will increase Addivox's CPU usage.

Drive, tone and chorus are saved in patches (see [Patches](patches.md)). Reverb is a persistent control, so it holds its value when patches change.

These are simple one-knob effects, intended mainly to give the standalone version of Addivox a finished sound. When using Addivox as a plugin in a DAW, you may prefer to leave them off and use dedicated effects plugins instead. For that reason, reverb defaults to 50% for standalone versions of Addivox and 0% for plugin versions of Addivox.
