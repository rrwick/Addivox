# Release notes

### Availability of past versions

When new versions of Addivox are released, they will be made available through Lemon Squeezy to both new and existing customers.

New versions of Addivox are of course intended to improve stability and functionality, but there may be cases where an older version works better for your setup. Previous versions of Addivox are therefore available to customers on request. If you need an older version, please [email me](mailto:addivox.support@gmail.com) using the email address used for purchase, or provide your Lemon Squeezy order number.



### [Addivox v1.1.0 – 25 September 2026](https://github.com/rrwick/Addivox/releases/tag/v1.1.0)

Updates in this release:

- Added new factory patches.
- New macro knobs in some edit tabs (Level, Breath, Attack and Release) for easier editing.
- Extended the pitch bend range options to include off, 2 octaves and 4 octaves.
- Added a CC 65 option for portamento (binary on/off behaviour).
- Extended per-harmonic pitch offsets for more flexibility with inharmonic sounds.
- Increased minimum macOS version to 10.14. Running on an earlier OS should now result in a "not supported" message instead of an "Addivox quit unexpectedly" crash.
- Flipped the pan shift in the visualisation to be consistent with the Pan edit tab.
- Fixed a bug where custom names in user-defined patches weren't shown in the menu.
- Guard against a possible crash when MIDI initialization fails on startup.
- Fixed between-key-note interpolation for per-harmonic levels.
- Simplifications and fixes for the Audio & MIDI Settings dialog.

Full version:

- `Addivox_v1.1.0_macOS.zip`<br>SHA-256 = `7d6d7e5c815ab567971977e02045d2964b008bf137a83bdd2071db12258286e6`
- `Addivox_v1.1.0_Windows.zip`<br>SHA-256 = `a521993b4f8409bffaeddb32b7e68ad0b6e34de8054a614d7bff6987b82eee38`

Demo version:

- [`AddivoxDemo_v1.1.0_macOS.zip`](https://github.com/rrwick/Addivox/releases/download/v1.1.0/AddivoxDemo_v1.1.0_macOS.zip)<br>SHA-256 = `206cdaecfab6541513fe8fe617a31bd122331e70cf1ac620870bafeea2019d9f`
- [`AddivoxDemo_v1.1.0_Windows.zip`](https://github.com/rrwick/Addivox/releases/download/v1.1.0/AddivoxDemo_v1.1.0_Windows.zip)<br>SHA-256 = `f30170e6e7ee356337d1e8ecda40a40815dbc45b63110acfca85b654f2d9f634`



### [Addivox v1.0.2 – 14 July 2026](https://github.com/rrwick/Addivox/releases/tag/v1.0.2)

Updates in this release:

- Fixed a bug where settings that aren't part of a patch (tuning, pan and reverb) could reset when changing patches.
- Attack and release now have a small minimum time, so the shortest settings no longer produce harsh clicks.
- Smoother breath response: changing breath level with very low attack/release settings no longer causes zipper noise.

Full version:

- `Addivox_v1.0.2_macOS.zip`<br>SHA-256 = `474931fb3aaccb8206650353d75efd434172f434848d52efc0115f0a1b87d558`
- `Addivox_v1.0.2_Windows.zip`<br>SHA-256 = `c35d6a9753baf3427fcd952c7e3356322ae9d1d56f2cada75fe84b6548ffe559`

Demo version:

- [`AddivoxDemo_v1.0.2_macOS.zip`](https://github.com/rrwick/Addivox/releases/download/v1.0.2/AddivoxDemo_v1.0.2_macOS.zip)<br>SHA-256 = `15dbea500f7d1902fd28c68f350db2c7e59205b2dbc4f028a0000d97ac9c90e0`
- [`AddivoxDemo_v1.0.2_Windows.zip`](https://github.com/rrwick/Addivox/releases/download/v1.0.2/AddivoxDemo_v1.0.2_Windows.zip)<br>SHA-256 = `2da503440222d6b6ed63fbbb3128d0c55b6b4abc9533e55f5d1194c27868880d`



### [Addivox v1.0.1 – 11 July 2026](https://github.com/rrwick/Addivox/releases/tag/v1.0.1)

Some bug fixes:

- Fixed the AUv3 plugin failing to load correctly in some hosts.
- Fixed the AUv3 plugin's window opening at the wrong size.
- Fixed Objective-C class collisions between the full and demo versions.
- Fixed a rare crash caused by a state-handling race condition.
- Fixed a rare crash when closing the plugin window while a menu was open.

Full version:

- `Addivox_v1.0.1_macOS.zip`<br>SHA-256 = `60ed98cea0c76010156d44fe75675bbec99d5bedd78551ec9e74dea07d6fb34c`
- `Addivox_v1.0.1_Windows.zip`<br>SHA-256 = `3fe84773ff661b6a76b5b98ece27a0df001d243d5d0a917b2f4a2450fcf234bf`

Demo version:

- [`AddivoxDemo_v1.0.1_macOS.zip`](https://github.com/rrwick/Addivox/releases/download/v1.0.1/AddivoxDemo_v1.0.1_macOS.zip)<br>SHA-256 = `8e105f58ebb3f64579bc084cfcdc5f42c090bd03bd4c2f20d1139dbc506f61b4`
- [`AddivoxDemo_v1.0.1_Windows.zip`](https://github.com/rrwick/Addivox/releases/download/v1.0.1/AddivoxDemo_v1.0.1_Windows.zip)<br>SHA-256 = `bbe731a8d8791a0becde740a7a2a22f39a51af5e2f1000250b65d60ffb631e55`



### [Addivox v1.0.0 – 09 July 2026](https://github.com/rrwick/Addivox/releases/tag/v1.0.0)

The very first release of Addivox 😁

Full version:

- `Addivox_v1.0.0_macOS.zip`<br>SHA-256 = `9311005a869175a3ff7e7ff67871d6dbf315b344b544449195ebe8e26b784a29`
- `Addivox_v1.0.0_Windows.zip`<br>SHA-256 = `03591aca4a137fd9adf68509868935dd866dc8e74ea2544dff31a00c92371a00`

Demo version:

- [`AddivoxDemo_v1.0.0_macOS.zip`](https://github.com/rrwick/Addivox/releases/download/v1.0.0/AddivoxDemo_v1.0.0_macOS.zip)<br>SHA-256 = `8eb0308473a1035f51ad222ac4a4986a02cfac8c146fccca7bc23bdeec908460`
- [`AddivoxDemo_v1.0.0_Windows.zip`](https://github.com/rrwick/Addivox/releases/download/v1.0.0/AddivoxDemo_v1.0.0_Windows.zip)<br>SHA-256 = `a7f1079319d8a000903cfef9009ea2338fb51d590abd4e5da9cf5c9ec5d14183`
