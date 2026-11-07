# Patches

### Factory patches

Addivox ships with 23 factory patches:

- **Init Patch** — A simple one-key-note patch with default values for all edit-tab macros. A good starting point for building your own patch.

- **Bright Brass** — A clean bright brassy sound, ranging from tuba-like to trombone-like to trumpet-like. Has a smoothly tapering Level curve, and for lower notes, the peak is at a higher harmonic number.

- **Warm Brass** — Similar to Bright Brass, except the Level curve peaks sooner and tapers faster, giving fewer high harmonics.

- **Mellow Brass** — Similar to Warm Brass, except the Level curve peaks sooner and tapers faster, giving even fewer high harmonics.

- **Dark Brass** — Similar to Mellow Brass, except the Level curve peaks sooner (first harmonic is always strongest) and tapers faster, giving even fewer high harmonics.

- **Bright Reed** — A bright woodwind-like sound with strong formants across the entire spectrum.

- **Warm Reed** — Similar to Bright Reed, but the Level curve peaks sooner giving weaker high harmonics and with different formant peaks.

- **Hollow Reed** — Similar to Warm Reed, but with an odd-harmonic bias for a clarinet-like sound and with different formant peaks.

- **Tonewheel Organ 888** — Inspired by a Hammond organ with drawbars set to 888000000. Contains a generous amount of level, pan and pitch variation for a dynamic sound.

- **Tonewheel Organ Full** — Inspired by a Hammond organ with drawbars set to 888888888. Contains a generous amount of level, pan and pitch variation for a dynamic sound.

- **Full Pipe Organ** — Has harmonics at equal levels across the octaves for a big full-spectrum sound. Contains a bit of level, pan and pitch variation.

- **Simple Saw** — Level curve follows a 1/*h* shape, approximating a sawtooth wave. The top harmonics taper to zero to prevent a high-pitched whine sound at full breath.

- **Simple Square** — Level curve follows a 1/*h* shape, odd harmonics only, approximating a square wave. The top harmonics taper to zero to prevent a high-pitched whine sound at full breath.

- **Simple Sine** — Just a single oscillator at the fundamental. As simple as it gets!

- **Warm Fuzz** — A warm dynamic sound with an even harmonic bias to create an octave-doubling effect.

- **Power Chord** — Harmonics which correspond to a root and fifth played together, with lots of drive.

- **Choir Ah** — Strong formants to create an "Ah" vowel sound.

- **Choir Oh** — Strong formants to create an "Oh" vowel sound.

- **Choir Wow** — A vowel sound that changes with breath.

- **Church Bell** — A bell sound with many inharmonic partials and a slow release time.

- **Low Pass Resonance** — A resonant low-pass sweep controlled by breath.

- **Band Pass Resonance** — A band-pass sweep controlled by breath.

- **Double Peak Resonance** — Two resonant peaks that move with breath.

Some patches include Formants or Resonance curves that are off by default. Turn up the [global knobs](global_settings.md) to try them.


### Custom patches

You can create and save your own patches from within Addivox. Custom patches are stored in Addivox's patches folder:

- **macOS:** `~/Library/Application Support/Addivox/Patches/`
- **Windows:** `%LOCALAPPDATA%\Addivox\Patches\`
<!-- - **iOS:** `On My iPad/Addivox/Addivox Patches/` -->

For the demo version, these folders are named `AddivoxDemo` instead of `Addivox`.

Some plugin formats run in a sandbox (for example, AUv3 on macOS). A sandboxed plugin keeps its own separate patches folder, so patches saved there won't automatically appear in the standalone app or in other plugin formats. If a patch seems to be missing, this is the most likely reason — use the "Show User Patches in Finder" command in the patch menu to see exactly which folder that instance of Addivox is using, and copy patch files between folders as needed.

Patches placed in subdirectories of this folder appear in their own named group in the patch menu, which you can use to organise larger collections.


### Patch file format

Patches are plain-text [TOML](https://toml.io) files. Here is an abbreviated example of a single-key-note patch:

```toml
format_version = 2
name = "My Patch"

[voice_settings]
levelScale = 1.0
formantsScale = 1.0
resonanceScale = 0.0
attackScale = 1.0
releaseScale = 1.0
levelVariationAmplitudeScale = 0.0
levelVariationRateScale = 1.0
pitchVariationAmplitudeScale = 0.0
pitchVariationRateScale = 1.0
panVariationAmplitudeScale = 0.0
panVariationRateScale = 1.0
portamentoTimeAtCC5MinSec = 0.001
portamentoTimeAtCC5MaxSec = 0.025

[effects_settings]
drive = 0.0
tone = 0.0
chorus = 0.0

[[key_notes]]
midi_note = 60
note_name = "C4"
level         = [0.9, 0.8, 0.7, ...]
breath_power  = [1.0, 1.0, 1.0, ...]
attack        = [0.005, ...]
release       = [0.01, ...]
pitch         = [0.0, ...]
pan           = [0.0, ...]
level_variation_amplitude = [0.25, ...]
level_variation_rate      = [1.0, ...]
pan_variation_amplitude   = [0.25, ...]
pan_variation_rate        = [1.0, ...]
pitch_variation_amplitude = [5.0, ...]
pitch_variation_rate      = [1.0, ...]
formants_freq_hz = []
formants_db      = []
resonance_zero_freq_hz = []
resonance_zero_db      = []
resonance_full_freq_hz = []
resonance_full_db      = []
resonance_breath_response = 0.0
resonance_tracking = 0.0
```

Each per-harmonic array contains exactly 100 values (one per harmonic). A patch with multiple key notes has one `[[key_notes]]` section for each. An optional `[all_key_notes]` section stores parameters that are locked in sync across all key notes (see [Key Notes and Interpolation](key_notes_and_interpolation.md)).

Because patches are plain text, you can open and edit them in any text editor. As long as the file is correctly formatted and has a `.toml` extension, Addivox will load it correctly.
