# Addivox

This `noise` branch of Addivox contains an experimental feature: noise on note attack and/or note sustain. Noise is produced with additive synthesis using dense inharmonic sine-wave components spread across 100 frequency bands. Each active band contains 20 noise components with random frequency, phase, pan and lifespan. Noise sustains correlates with the breath, and noise attack correlates with the first derivative of the breath. The idea was to add breathy or percussive sounds to notes, and it sort of worked.

But I decided to not include noise in Addivox (at least for the moment) for the following reasons:
* The densely packed oscillators that it uses to make noise can add up: thousands of oscillators for full-spectrum noise. This can use a lot of CPU, so I worry that it won't be compatible with older computers or mobile platforms.
* It wasn't easy to make the noise sound nice and musical. It's possible, takes a lot of very fine tuning, so isn't user-friendly.

So the main branch of Addivox has this feature removed, but I'm keeping it here in a separate branch in case I want to come back to it later.
