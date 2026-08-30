# EWI fingerings

<div class="ewi-page">
  <div class="ewi-instrument">
    <div class="ewi-note" id="ewi-note">—</div>
    <input class="ewi-code" id="ewi-code" type="text" autocomplete="off"
           spellcheck="false" aria-label="Fingering code"
           title="The current fingering's code. Type or paste one to load it.">
    <div class="ewi-diagram" id="ewi-diagram">Loading…</div>
  </div>
  <div class="ewi-actions">
    <div class="ewi-controls">
      <label for="ewi-model">Key layout:</label>
      <select id="ewi-model"></select>
      <label class="ewi-check"><input type="checkbox" id="ewi-mirror"> Mirror</label>
    </div>
    <div class="ewi-buttons">
      <button class="ewi-button" id="ewi-add" type="button">Add fingering</button>
      <button class="ewi-button" id="ewi-remove" type="button">Remove fingering</button>
      <select class="ewi-set" id="ewi-set">
        <option value="" selected disabled hidden>Load set…</option>
        <option value="clear">Clear</option>
        <option value="all">All possible fingerings</option>
      </select>
      <button class="ewi-button" id="ewi-copy" type="button">Copy link</button>
    </div>
    <p class="ewi-status" id="ewi-status" hidden></p>
  </div>
  <div class="ewi-table-scroll">
    <div class="ewi-table" id="ewi-table"></div>
  </div>
</div>

This page allows you to explore the wonderfully flexible world of EWI fingerings! With 13 keys, there are 2<sup>13</sup> = 8192 possible fingerings in total. An individual player will likely only use a small subset of these, but different players may settle on different subsets.

On this page, you can add the fingerings you use, and then use the "Copy link" button to share them with others or save them for later. Nothing is saved on this site (it does not use cookies), so use "Copy link" if you want to keep your fingering set.

Use the "Key layout" dropdown to change the fingering appearance. The "Mirror" toggle flips right and left, which may be useful for visualising the finger positions.

You can also browse some existing fingering sets with the "Load set" dropdown. Some of these came from the fingering section of instrument manuals, and while they often agree on some notes (e.g. F), they differ on other notes (e.g. B♭). Others came from various resources on the web – see the Credits section below.


### Fingering logic

The keys are named with "LH" or "RH" for left-hand or right-hand. The keys with "p" in the name are typically played with the pinky finger. Some keys have a fixed effect while some interact with other keys.

- LH1 (a.k.a. B key): −2 semitones
- LHb (a.k.a. bis key): −1 semitone, but does nothing if both LH1 and LH2 are pressed
- LH2 (a.k.a. A key): −1 semitone, but −2 semitones if LH1 is pressed
- LH3 (a.k.a. G key): −2 semitones
- LHp1: +1 semitone
- LHp2: −1 semitone
- RHs (a.k.a. side key): +1 semitone, but does nothing if LHp1 is pressed
- RH1 (a.k.a. F key): −1 semitone, but −2 semitones if LH3 is pressed
- RH2 (a.k.a. E key): −1 semitone
- RH3 (a.k.a. D key): −2 semitones
- RHp1: +1 semitone
- RHp2: −1 semitone
- RHp3: −2 semitones
{ .ewi-tight }

This page uses C♯ as the reference note, i.e. the note played when no keys are pressed. This should correspond to default settings (no transposition) on supported instruments. This page also does not model octave settings, which are often controlled with the thumb.

Some instruments are lacking one or more of these 13 keys. For these key layouts, the unusable keys are displayed to the side with a dashed border and are coloured red when pressed. They can still be interacted with, but any fingering that uses one of these keys is not possible on that instrument, and these fingerings will be coloured red in the table. See the compatibility notes below for more details.

Some instruments have keys in addition to the 13 used in this logic. For these key layouts, the extra keys are drawn faintly and are not interactive. See the compatibility notes below for more details.

13 keys correspond to a 13-bit binary value. For example, the common F♯ fingering could be written as `1011000010000`, where `1` means pressed and `0` means not pressed. To make these codes more concise, this page converts them into [Crockford's base-32](https://en.wikipedia.org/wiki/Base32#Crockford's_Base32) which can represent each fingering using just three characters (`5GG` for the common F♯ fingering).



### Instrument compatibility

I don't own or have access to most of these instruments. If you do and you find any omissions or mistakes, please [let me know](mailto:addivox.support@gmail.com) and I'll update this page.

#### Compatible

- **Berglund NuRAD: EWI fingering.** I'm 100% certain the NuRAD EWI fingering works with this page, because I've read its [source code](https://github.com/berglundinst/NuEVI/blob/56d8568dd7883337e0929a4ed06e2894506ec2cc/NuEVI/NuEVI.ino#L2496). The NuRAD also has extra keys not used on this page: mod, LHp3 and special keys.
- **Akai EWI Solo: EWI fingering.** I own an EWI Solo, and while I haven't tried all 8192 fingerings, I haven't found any exceptions. The EWI Solo also has an extra F♯ key not used on this page.
- **Akai EWI4000, EWI5000 and EWI USB: EWI fingering.** I don't own any of these, but I am assuming they are consistent with the EWI Solo. Also, the logic described by Bret Pimentel in [this blog post](https://bretpimentel.com/flexible-ewi-fingerings/) is exactly the same as the logic on this page.


#### Probably/mostly/incompletely compatible

- **Robkoo Clarii PRO C20: EWI fingering.** All of the "EWI fingering" examples in the Clarii PRO C20 manual are compatible, but the manual only describes the LHb key in combination with LH1, so I'm not sure if it behaves the same on its own. Also, the RHs key (\*2 key in the Clarii PRO C20 manual) needs to be set to the "sharp" function to be compatible.
- **Greaten AP100: standard fingering.** This instrument lacks RHp1 and RHp2 keys. Otherwise, the "standard fingering" set in its manual seems to be compatible.
- **Greaten AP300: standard fingering.** The "standard fingering" set in its manual seems to be compatible.
- **Greaten AP500: standard fingering.** This instrument lacks the LHb key. Otherwise, the "standard fingering" set in its manual seems to be compatible. It also has an extra "functional" key not used on this page.
- **Aodyo Sylphyo: EWI fingering.** This instrument lacks the LHb, LHp2, RHs, RHp1 and RHp2 keys.

#### Incompatible

- **Roland Aerophone series.** These have an "Electronic wind" fingering mode, where many of the keys follow the logic on this page. However, some of the additional keys (operated by index and pinky fingers) behave differently, making the Aerophone range incompatible with this page.
- **Berglund NuEVI.** This instrument is based around brass fingerings, a very different system to the EWI fingerings.
- **Robkoo R1 and Clarii mini.** Do not have an EWI-like fingering mode.
- **Yamaha WX5.** Does not have an EWI-like fingering mode.
- **Yamaha YDS-120 and Yamaha YDS-150.** Do not have an EWI-like fingering mode.
- **EMEO.** Does not have an EWI-like fingering mode.
- **Diosynth.** Does not have an EWI-like fingering mode.


### Credits

The "Curt Sipe transposable" fingerings were inspired by [these scale books](https://www.curtsipe.com/books-sales) created by Curt Sipe. They were very influential on my fingering choices when I started to learn the EWI. The idea is that by not using pinky keys, these fingerings allow you to transpose your playing up or down a few semitones by holding one or more pinky keys while you play. While I do not usually play with transposing (I just play natively in each key), I still enjoy the simplicity of these fingerings, and it leaves the pinky keys free for things like trills. These fingerings are also compatible with all of the key layouts on this site (except for the bis-B♭ fingering on layouts lacking LHb). Curt also has lots of EWI content, including videos about fingerings, on his [YouTube channel](https://www.youtube.com/@curtsipe-forge).

The "Bret Pimentel borrowed" fingerings were taken from [this post](https://bretpimentel.com/using-borrowed-fingerings-in-ewi-mode/). They include the fingerings from the EWI4000/EWI5000 manual plus eight more borrowed from other instruments.

The "Bernie Kenerson little finger" fingerings were taken from [this post](https://berniekenerson.com/blogs/ewi-tips-techniques/posts/6164084/51-ewi-little-finger-exercises-free-download). They include the fingerings from the EWI4000/EWI5000 manual plus additional ones that use the pinky keys to avoid octave breaks. The post also includes sheet music showing which fingerings to use in which contexts.

The "Eric Marx listing" is a particularly large fingering set taken from [this PDF](https://www.patchmanmusic.com/EWIFingeringOneWatts.pdf).

The "Yoshimeme style" fingering set was taken from [this PDF](https://www.alsoj.net/download/ewi/ALEWISG1_fingeringchart.pdf). Yoshimeme is a Japanese EWI player and teacher who published an [EWI Start Guide](https://gakufubin.shimamura.co.jp/ec/pro/disp/2/g0252972) with these fingerings.


<script src="../assets/ewi_fingerings/ewi_fingerings.js"></script>
