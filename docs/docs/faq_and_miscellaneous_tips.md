# FAQ and Miscellaneous Tips


### What are the system requirements for Addivox?

I can't give specific minimum specs, since performance depends on your DAW, buffer size and whatever else you're running alongside Addivox. But as a couple of data points, it runs fine on an M1 MacBook Air (2020) and on an Intel Core i3-8109U (2018), and it doesn't use much memory.

For Macs, both Intel and Apple Silicon chips are supported, but your graphics hardware will need to support [Metal](https://en.wikipedia.org/wiki/Metal_(API)). Addivox requires macOS 10.14 (Mojave) or later, which guarantees Metal support. This rules out most Macs from about 2011 or earlier.

If your hardware is older or less powerful, you can reduce CPU usage by disabling the visualizer in the [settings menu](main_ui.md#settings-menu). The best way to know for sure: download the [demo version of Addivox](demo.md) and try it on your system before buying.

<!-- * It runs fine on my 9th gen iPad from 2021, and doesn't max out the CPU.
* So I predict it will do okay with iPads from about 2019 onward. -->


### Which Windows audio driver type should I use?

This only applies to the standalone Windows application. If you use Addivox as a VST3 or CLAP plugin, your DAW handles the audio driver.

On a new Windows installation, Addivox starts with DirectSound unless you choose something else. You can change the driver type in the standalone app's Audio & MIDI Settings.

For real-time playing, **ASIO** is usually the best choice. If your audio interface has a native ASIO driver from its manufacturer, use that. Native ASIO drivers usually give the lowest latency and most reliable performance. If you do not have a native ASIO driver, [ASIO4ALL](https://asio4all.org/) can be worth trying.

**DirectSound** is the basic Windows option. It is useful as a fallback and often works without extra setup, but it may have too much latency for live playing.

**WASAPI** is another built-in Windows option. Addivox supports WASAPI shared mode, which allows other applications to keep using the audio device at the same time. On some systems it may work well, but it is not always lower-latency than DirectSound.


### Clipping audio on macOS and Windows

Addivox can produce floating-point audio above 0 dBFS, especially if a patch has high per-harmonic levels, is played at full breath, uses **Level variation** or if the global **Level** knob is turned up. The **Main Out** meter shows peak output level; if it reaches the red range, the signal is at or above 0 dBFS and may clip somewhere after Addivox.

Whether or not an output level > 0 dBFS will cause clipping distortion seems to depend on the platform and audio driver. In my experience, macOS (Core Audio) and Windows WASAPI are tolerant of high levels, and I do not hear distortion when Addivox exceeds 0 dBFS. But with Windows DirectSound and Windows ASIO, I do hear clipping distortion.

The safest habit is to watch the **Main Out** meter during playing, and if you see it reach the red range, reduce the overall level, either via the [per-harmonic](per-harmonic_settings.md) Level tab or the [global](global_settings.md) Level knob.


### Is a Pro Tools (AAX) plugin available?

No, Addivox is not currently available as an AAX plugin for Pro Tools.

iPlug2 can build AAX plugins, so this is possible in principle. However, public Pro Tools builds require AAX plugins to be signed through Avid/PACE's signing system before they can be loaded. I also do not own Pro Tools set up for proper testing, so I would not be able to test any AAX builds I made.


### Can Addivox be played with a keyboard?

Sure, though Addivox is built around breath control, so played from a plain keyboard with no breath input, it will sound flat. The easiest fix is a mod wheel (CC 1), since many keyboards have one built in. A breath controller or an expression/volume pedal linked to CC 2, CC 7 or CC 11 works too. I haven't tried playing Addivox with a keyboard myself, so your mileage may vary. Velocity and aftertouch are not currently supported as a breath source — if that's a feature you'd like, please [let me know](support_and_feedback.md). One more thing to keep in mind: Addivox is a monosynth, so it can only play one note at a time, which rules out chords.


### Can Addivox mimic real instrument sounds?

Perhaps, with some careful fine-tuning of the patches, but this is not what Addivox was designed for. If your goal is to mimic a real instrument with your wind controller, then something like the [SWAM instruments](https://audiomodeling.com) would be a better choice.


### Can Addivox produce breathy flute sounds?

No, it cannot. Addivox's synthesis is based on summing harmonic sine waves, and it does not have the ability to create noise (non-pitched sound) in its synthesised output. If you are interested in breathy flute sounds, I have found [Respiro](https://www.kvraudio.com/product/respiro-by-imoxplus) (a physical-modelling synthesizer) to excel at this.


### Why additive synthesis?

There are many ways to synthesize sound, so why is Addivox built around additive synthesis? The biggest reason is control: independent level, breath response, attack/release, pitch, pan and variation for each of the 100 harmonics. This allows for some cool effects that are not easy with other synthesis methods, e.g. the pitch variation I use in the organ-style presets.

Additive synthesis also makes it easy to implement [formants](https://en.wikipedia.org/wiki/Formant). During synthesis, each harmonic is scaled based on its frequency and the [EQ curve](eq.md), rather than rendering a waveform first and processing it with an EQ filter afterwards, which is less precise.

Since Addivox always knows the exact frequency of every harmonic, it can simply skip rendering any harmonic above the [Nyquist frequency](https://en.wikipedia.org/wiki/Nyquist_frequency), which solves aliasing. The same certainty is what makes the visualization possible too: it shows the exact level of all 100 harmonics in real time, without the smearing of an FFT-based spectrum analyser.

The main cost is CPU usage: 100 explicit oscillators per note is more expensive than a typical synth voice. But Addivox is a monosynth (one note at a time), so that cost doesn't scale with polyphony.


### What was the inspiration for Addivox?

Like many who play wind controllers, I enjoyed the brassy sounds of the [EVI-NER](https://www.davidsonaudioandmultimedia.com/products/evi-ner) synthesizer. However, I had a couple issues with EVI-NER. First of all, its sound was a bit too synthesizery, especially in the low end. I wanted something similar to a brass sound over the full range, from tuba-like in the low range to trombone-like in the mid range to trumpet-like in the upper range. Second, while I was able to register my copy of EVI-NER and get my serial number, I have seen many reports online of people who could not, so it seems that EVI-NER may not be a reliable option for the windsynth community.

Addivox solves the first issue by allowing for multiple [key notes](key_notes_and_interpolation.md) in its patches. This allows you to define different sounds in different registers, creating patches that smoothly span many octaves. And Addivox solves the second issue by being source-available (see [License](license.md)) and not containing any DRM like serial numbers or activation codes.

Finally, while I did not start developing Addivox with formants in mind, during development I learned how important they can be for shaping the timbre of a sound. So I now consider the ability to apply formant EQ filters to be one of Addivox's key features (see [EQ](eq.md)).
