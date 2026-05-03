# FAQ and Miscellaneous Tips


## What was the inspiration for Addivox?

Like many who play wind controllers, I enjoyed the brassy sounds of the [EVI-NER](https://www.davidsonaudioandmultimedia.com/products/evi-ner) synthesizer. However, I had a couple issues with EVI-NER. First of all, its sound was a bit too synthesizery, especially in the low end. I wanted something that could approximate a brass sound over the full range, from tuba-like in the low range to trombone-like in the mid range to trumpet-like in the upper range. Second, while I was able to register my copy of EVI-NER and get my serial number, I have seen many reports online of people who could not, so it seems that EVI-NER may not be a reliable option for the windsynth community.

Addivox solves my first issue by allowing for multiple key notes in its patches. This allows you to define different sounds in different registers, creating patches that smoothly span many octaves. And Addivox solves the second problem by being source-available (see [License](license.md)) and not containing any DRM like serial numbers or activation codes.

Finally, while I did not start developing Addivox with formants in mind, during development I learned how important they can be for shaping the timbre of a sound. So I now consider the ability to apply formant EQ filters to be one of Addivox's key features (see [EQ](eq.md)).


## Can Addivox mimic real instruments sounds?

Perhaps, with some careful fine-tuning of the patches, but this is not what Addivox was designed for. If your goal is to mimic a real instrument with your wind controller, then something like the [SWAM instruments](https://audiomodeling.com) would be a better choice.


## Can Addivox produce breathy flute sounds?

No, it cannot. Addivox's synthesis is based on summing harmonic sine waves, and it does not have the ability to create noise in its synthesised sound. If you are interested in breathy flute sounds, I have found [Respiro](https://www.kvraudio.com/product/respiro-by-imoxplus) (physical-modelling synthesizer) to excel at this.
