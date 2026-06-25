# FAQ and Miscellaneous Tips


## What was the inspiration for Addivox?

Like many who play wind controllers, I enjoyed the brassy sounds of the [EVI-NER](https://www.davidsonaudioandmultimedia.com/products/evi-ner) synthesizer. However, I had a couple issues with EVI-NER. First of all, its sound was a bit too synthesizery, especially in the low end. I wanted something more similar to a brass sound over the full range, from tuba-like in the low range to trombone-like in the mid range to trumpet-like in the upper range. Second, while I was able to register my copy of EVI-NER and get my serial number, I have seen many reports online of people who could not, so it seems that EVI-NER may not be a reliable option for the windsynth community.

Addivox solves the first issue by allowing for multiple key notes in its patches. This allows you to define different sounds in different registers, creating patches that smoothly span many octaves. And Addivox solves the second issue by being source-available (see [License](license.md)) and not containing any DRM like serial numbers or activation codes.

Finally, while I did not start developing Addivox with formants in mind, during development I learned how important they can be for shaping the timbre of a sound. So I now consider the ability to apply formant EQ filters to be one of Addivox's key features (see [EQ](eq.md)).


## Can Addivox mimic real instruments sounds?

Perhaps, with some careful fine-tuning of the patches, but this is not what Addivox was designed for. If your goal is to mimic a real instrument with your wind controller, then something like the [SWAM instruments](https://audiomodeling.com) would be a better choice.


## Can Addivox produce breathy flute sounds?

No, it cannot. Addivox's synthesis is based on summing harmonic sine waves, and it does not have the ability to create noise in its synthesised sound. If you are interested in breathy flute sounds, I have found [Respiro](https://www.kvraudio.com/product/respiro-by-imoxplus) (a physical-modelling synthesizer) to excel at this.


## What are the system requirements for Addivox?

* It runs fine on my 9th gen iPad from 2021, and doesn't max out the CPU.
* So I predict it will do okay with iPads from about 2019 onward.
* Need to test on an M1 Mac.
* If your hardware is older, you can disable the visualization to reduce CPU usage.
* Also, try before you buy! Get the demo version and make sure it works on your hardware before you purchase Addivox.


## Which Windows audio driver type should I use?

This only applies to the standalone Windows application. If you use Addivox as a VST3 or CLAP plugin, your DAW handles the audio driver.

On a new Windows installation, Addivox starts with DirectSound unless you choose something else. You can change the driver type in the standalone app's Audio & MIDI Settings.

For real-time playing, **ASIO** is usually the best choice. If your audio interface has a native ASIO driver from its manufacturer, use that. Native ASIO drivers usually give the lowest latency and most reliable performance. If you do not have a native ASIO driver, [ASIO4ALL](https://asio4all.org/) can be worth trying.

**DirectSound** is the basic Windows option. It is useful as a fallback and often works without extra setup, but it may have too much latency for live playing.

**WASAPI** is another built-in Windows option. Addivox supports WASAPI shared mode, which allows other applications to keep using the audio device at the same time. On some systems it may work well, but it is not always lower-latency than DirectSound.


## Is a Pro Tools (AAX) plugin available?

No, Addivox is not currently available as an AAX plugin for Pro Tools.

iPlug2 can build AAX plugins, so this is possible in principle. However, public Pro Tools builds require AAX plugins to be signed through Avid/PACE's signing system before they can be loaded. I also do not own Pro Tools set up for proper testing, so I would not be able to test any AAX builds I made.
