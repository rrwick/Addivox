# Patches

### Factory patches

Addivox ships with 13 factory patches:

<table>
  <colgroup>
    <col style="width: 220px;">
    <col>
  </colgroup>
  <tbody>
    <tr>
      <td><img src="../assets/patch_01_bright_brass.png" alt="Bright Brass" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Bright Brass</strong><br>A clean bright brassy sound, ranging from tuba-like to trombone-like to trumpet-like. Has a smoothly tapering Level curve, and for lower notes, the peak is at a higher harmonic number. No formants.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_02_warm_brass.png" alt="Warm Brass" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Warm Brass</strong><br>Similar to Bright Brass, except the Level curve peaks sooner and tapers faster, giving fewer high harmonics.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_03_mellow_brass.png" alt="Mellow Brass" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Mellow Brass</strong><br>Similar to Warm Brass, except the Level curve peaks sooner and tapers faster, giving even fewer high harmonics.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_04_dark_brass.png" alt="Dark Brass" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Dark Brass</strong><br>Similar to Mellow Brass, except the Level curve peaks sooner (first harmonic is always strongest) and tapers faster, giving even fewer high harmonics.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_05_bright_reed.png" alt="Bright Reed" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Bright Reed</strong><br>A bright woodwind-like sound with strong formants across the entire spectrum.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_06_warm_reed.png" alt="Warm Reed" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Warm Reed</strong><br>Similar to Bright Reed, but the Level curve peaks sooner giving weaker high harmonics and with different formant peaks.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_07_hollow_reed.png" alt="Hollow Reed" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Hollow Reed</strong><br>Similar to Warm Reed, but with an odd-harmonic bias for a clarinet-like sound and with different formant peaks.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_08_tonewheel_organ_888.png" alt="Tonewheel Organ 888" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Tonewheel Organ 888</strong><br>Inspired by a Hammond organ with drawbars set to 888000000. Contains a generous amount of level, pan and pitch variation to make the notes sound more lively.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_09_tonewheel_organ_full.png" alt="Tonewheel Organ Full" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Tonewheel Organ Full</strong><br>Inspired by a Hammond organ with drawbars set to 888888888. Contains a generous amount of level, pan and pitch variation to make the notes sound more lively.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_10_full_pipe_organ.png" alt="Full Pipe Organ" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Full Pipe Organ</strong><br>Has harmonics at equal levels across the octaves for a big full-spectrum sound. Contains a bit of level, pan and pitch variation.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_11_simple_saw.png" alt="Simple Saw" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Simple Saw</strong><br>Level curve follows a 1/<em>h</em> shape, approximating a sawtooth wave. The top harmonics taper to zero to prevent a high-pitched whine sound at full breath for low notes.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_12_simple_square.png" alt="Simple Square" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Simple Square</strong><br>Level curve follows a 1/<em>h</em> shape, odd harmonics only, approximating a square wave. The top harmonics taper to zero to prevent a high-pitched whine sound at full breath for low notes.</td>
    </tr>
    <tr>
      <td><img src="../assets/patch_13_simple_sine.png" alt="Simple Sine" style="width: 300px; max-width: 100%;"></td>
      <td><strong>Simple Sine</strong><br>Just a single oscillator at the fundamental. As simple as it gets!</td>
    </tr>
  </tbody>
</table>


### Custom patches

You can create and save your own patches from within Addivox. Custom patches are stored as TOML text files in the **Addivox Patches** folder:

- **macOS:** `~/Library/Application Support/Addivox/Patches/`
- **Windows:** `%LOCALAPPDATA%\Addivox\Patches\`
<!-- - **iOS:** `On My iPad/Addivox/Addivox Patches/` -->

For the demo version, these folders are named `AddivoxDemo` instead of `Addivox`.

Patches placed in subdirectories of this folder appear in their own named group in the patch menu, which you can use to organise larger collections.


### Patch file format

Patches are plain-text [TOML](https://toml.io) files. Here is a minimal example of a single-key-note patch:

```toml
format_version = 1
name = "My Patch"

[voice_settings]
levelScale = 1.0
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
pitch_variation_amplitude = [5.0, ...]
pitch_variation_rate      = [1.0, ...]
pan_variation_amplitude   = [0.25, ...]
pan_variation_rate        = [1.0, ...]
eq_freq_hz = []
eq_db      = []
```

Each per-harmonic array contains exactly 100 values (one per harmonic). A patch with multiple key notes has one `[[key_notes]]` section for each. An optional `[all_key_notes]` section stores parameters that are locked in sync across all key notes (see [Key Notes and Interpolation](key_notes_and_interpolation.md)).

Because patches are plain text, you are welcome to open and edit them in any text editor. As long as the file follows the format above and has a `.toml` extension, Addivox will load it correctly.
