# UNVRS Audio Plugins

Sada 12 audio pluginů (VST3 / AU / Standalone) postavených na frameworku [JUCE](https://juce.com):
8 efektů a 4 nástroje.

| Plugin | Typ | Co dělá |
| --- | --- | --- |
| [S2K Converter](#s2k-converter) | efekt | emulace 16bitového rackového sampleru typu Akai S2000 |
| [Crest Limiter](#crest-limiter) | efekt | look-ahead brickwall limiter s LUFS metrem |
| [Velour](#velour) | efekt | barvící pasivní EQ s transformátorem a lampou |
| [Velour P](#velour-p) | efekt | parametrická verze Velouru se 6 pásmy a analyzérem |
| [Poise](#poise) | efekt | automatické spektrální vyvážení podle cílové křivky |
| [Ember](#ember) | efekt | overdrive ve stylu krabičky |
| [Nimbus](#nimbus) | efekt | algoritmický reverb s režimy shimmer a freeze |
| [Spool](#spool) | efekt | looper s overdubem, undo/redo a modifikátory |
| [Ictus](#ictus) | nástroj | fyzikální modelování: úderem buzené rezonátory |
| [Rudiment](#rudiment) | nástroj | analogově laděný bicí automat se sekvencerem |
| [Sextant](#sextant) | nástroj | 6operátorový FM syntezátor |
| [Pollen](#pollen) | nástroj | granulární syntezátor |

---

## Efekty

### S2K Converter

Emulace signálové cesty 16bitového rackového sampleru typu Akai S2000.

```
vstup → INPUT gain
      → analogový anti-alias filtr (Butterworth 8. řádu @ 0,45 × vzorkovací frekvence)
      → ADC: vzorkování 44,1 / 32 / 22,05 / 11,025 kHz, tvrdý clip, kvantizace 4–16 bit
      → sample engine: převod na 44,1 kHz (Drop / Linear / Hermite interpolace)
      → digitální 2pólový rezonanční low-pass (FILTER / RESO)
      → DAC: 16 bit @ 44,1 kHz
      → rekonstrukční filtr 20 kHz (nebo surový zero-order hold, když je DAC FILTER vypnutý)
      → analogový výstup: asymetrická saturace (DRIVE), šum (NOISE), DC-blokující kondenzátor
      → OUTPUT gain → MIX (dry/wet s kompenzací latence)
```

Celé se to počítá na ≥176 kHz (interní oversampling), aby převody mezi frekvencemi byly přesné.
Latence je konstantní a hlásí se hostiteli.

**Tipy**

- **Autentický S2000:** 44,1 kHz, 16 bit, Linear, oba filtry zapnuté, Drive ~20 %.
- **Typický „sampled“ zvuk 22 kHz:** 22,05 kHz – zaoblené výšky a víc grain.
- **Lo-fi / špinavé:** vypnout AA FILTER a DAC FILTER, interpolace Drop, 11 kHz, 8–12 bit.
- **Přebuzení ADC:** INPUT nahoru, dokud nesvítí ADC CLIP (tvrdý digitální clip jako na hardwaru).

**Omezení**

- Je to model postavený na známé architektuře přístroje (16 bit, 44,1/22,05 kHz, interpolace engine,
  2pólový filtr, analogový výstup). Není to měření skutečného kusu, takže konkrétní charakteristiky
  filtrů a saturace jsou odhad.
- Ladění vzorku nahoru a dolů (transpozice při přehrávání) neemuluje, protože efekt v reálném čase nemůže měnit délku signálu.
- Změna parametru Sample Rate resetuje převodník, takže při přepnutí může krátce lupnout.

### Crest Limiter

Brickwall limiter s předhledem (look-ahead) na finální stopu nebo master.

- Gain se do špiček „vklouzne“ plynule, místo aby skočil, a výstup nikdy nepřekročí CEILING.
- **Style:** Transparent / Punchy / Dense / Safe – různé chování releasu (dvoustupňový, závislý na materiálu).
- Parametry: GAIN, CEILING, LOOKAHEAD, RELEASE, stereo LINK.
- Oversampling Off / 2× / 4× / 8× proti inter-sample špičkám.
- Dither na 24 / 20 / 16 bit.
- Loudness metr podle ITU-R BS.1770 / EBU R128: momentary, short-term a integrovaná hlasitost (LUFS).
- **Unity Gain Listen:** odečte vstupní GAIN od výstupu, takže limiting posloucháš ve stejné hlasitosti a slyšíš, co dělá se zvukem.
- Latence je konstantní a nezávisí na nastavení lookahead.

### Velour

Barvící EQ ve stylu pasivních studiových ekvalizérů.

```
vstup → IRON (vstupní transformátor: saturace jádra v basech, jemný úbytek výšek)
      → pasivní EQ: HPF, LOW boost + LOW atten (stejná frekvence, vzájemně se ovlivňují),
        MID bell s proporcionálním Q, HIGH boost (šířka pásma), HIGH atten shelf
      → DRIVE (lampový make-up stupeň: asymetrický, sudé harmonické)
      → DC blok → OUTPUT
```

- Lampa je až za EQ, takže co přidáš v EQ, to ji víc budí.
- Vše za vstupem běží na 4× oversamplingu, filtry v double přesnosti.
- Přepínač **Analog** vypíná barvení (čistý EQ).

### Velour P

Parametrická verze Velouru: stejné IRON a DRIVE, ale moderní EQ.

```
vstup → IRON → HPF → 6 parametrických pásem → LPF → DRIVE → OUTPUT
```

- Každé pásmo: Bell / Low Shelf / High Shelf, zapínatelné, s frekvencí, ziskem a Q.
- HPF a LPF se strmostí 12 nebo 24 dB/okt.
- EQ křivka s tažitelnými body a spektrální analyzér za ní.
- **Console Q:** proporcionální Q u bell pásem jako na mixážním pultu (malé změny jsou široké, velké užší).
- Přepínač **Analog** vypíná transformátor a lampu (čistý EQ).
- 4× oversampling; parametry jsou vyhlazené, takže rychlá automatizace nezipuje.

### Poise

Automatické spektrální vyvážení. Sleduje tonální balanc stopy a průběžně ho tlačí k cílové křivce.

- **Target:** Pink / Mix / Warm / Bright / Learned (naučí se tvar z referenčního materiálu).
- **Amount** – jak moc se korekce aplikuje, **Keep Character** – kolik z vlastního dlouhodobého balancu stopy zachovat.
- **Speed**, **Resolution** (vyhlazení napříč frekvencemi v oktávách), **Range** (max. korekce v dB), **Tilt**.
- **Mode:** Cut + Boost / Cut Only / Boost Only; **Low/High Limit** omezí, ve kterém pásmu pracuje.
- Nikdy nezesiluje pásma, kde je skoro ticho. Auto Gain drží výstupní hlasitost.
- Analýza v pásmech po 1/3 oktávy, korekce bez fázového posunu (STFT), stejná pro L i R.

### Ember

Overdrive ve stylu kytarové krabičky.

```
vstup → pásmová propust (střed + šířka v oktávách)
      → 4× oversampled drive: gain → asymetrický soft clipper → zaoblovací stupeň
      → DC blok → DYNAMICS (vrací část původní dynamiky) → TONE → OUTPUT → MIX
```

- XY pad: osa X je střed pásma, osa Y jeho šířka.
- Asymetrický clipper zachovává sudé harmonické při jakémkoliv driveu.
- Dry/Wet s kompenzací latence.

### Nimbus

Algoritmický reverb na bázi 16linkové zpětnovazební sítě (FDN).

- **Character:** Room / Hall / Plate / Cathedral / Ambient.
- Predelay, Size, Decay (0,2–30 s), Diffusion, Bloom, Early reflections.
- Třípásmový decay: Bass Decay, Damping Freq, Treble Decay.
- Modulace (Depth, Rate), Low Cut, High Cut, Width, Ducking, Mix.
- **Shimmer** s intervalem +12, +7, +19, +24, −12 nebo +5 půltónů.
- **Freeze** – nekonečný dozvuk.
- Doba dozvuku je přesná a nezávislá na velikosti prostoru, síť je stabilní při jakémkoli nastavení.

### Spool

Looper s vrstvami, undo/redo a efekty přímo na smyčce.

- **Režimy:**
  - *Normal* – modifikátory barví jen to, co slyšíš.
  - *Additive* – overdub zapisuje i efekty zpátky do smyčky, takže se kumulují s každým průchodem.
  - *Sampler* – každé nahrání nahradí smyčku, trigger ji spustí od začátku.
- **Modifikátor A:** Smooth Speed / Dropper / Trimmer. **Modifikátor B:** Stepped Speed / Scrambler / Filter.
- **Repeats** – kolik ze smyčky zůstane při overdubu (feedback), **Stability** – kolísání jako u pásky (wow & flutter).
- Reverse, Dry Kill (vypne přímý signál), Dub After Rec (po nahrání rovnou pokračuje overdubem).
- **Sync:** Free / Beat / Bar – příkazy se kvantizují k tempu hostitele.
- **Ramp:** automatický náběh nebo bounce hlasitosti, repeats, modifikátorů nebo stability.
- Neomezené undo/redo v rámci paměti (3 minuty audia na kanál). Přepínání vrstev je bez lupnutí.
- Aktuální vrstvu umí uložit jako 24bit WAV do `~/Music/Spool`.
- **Ovládání MIDI notami** (od noty 36 / C1): Rec/Play, Stop, One-shot, Undo, Redo, Clear, Trigger,
  Mod A on/off, Mod B on/off, Reverse. V režimu Sampler podržení noty nahrává jen po dobu držení.

---

## Nástroje

### Ictus

Syntezátor s fyzikálním modelováním: budiče (palička + šum) rozezní dva modální rezonátory.

- **Rezonátory:** Beam / Marimba / String / Membrane / Plate / Pipe / Bell / Bowl, každý 32–128 módů.
- Parametry rezonátoru: ladění, Decay, Damp, Material, Shape, Inharm, Bright, místo úderu (Hit),
  místo snímání (Listen), Width.
- Zapojení rezonátorů paralelně, nebo sériově (1 → 2).
- **Budiče:** palička (tvrdost, citlivost na velocity, šum, barva) a filtrovaný šum s ADSR.
- Pitch envelope, polyfonie, velocity.
- 23 presetů: Marimba, Xylophone, Vibraphone, Glockenspiel, Kalimba, Music Box, struny, tabla,
  kotle, gong, zvony, singing bowl, flétny, wood block a další.

### Rudiment

Bicí automat s vlastní syntézou a vestavěným 16krokovým sekvencerem.

- **8 nástrojů:** Kick, Snare, Clap, Tom, Closed Hat, Open Hat, Perc, Cymbal – každý s vlastními parametry
  (ladění, sweep, decay, klik, drive, barva…), hlasitostí, panoramou a mute.
- Kick a tomy jsou modelované jako membrána, hi-haty a činely ze šesti inharmonických obdélníků jako u analogových automatů.
- **Sekvencer:** 16 kroků na nástroj s akcenty, vlastní délka každé stopy (polymetrie), swing.
  Sekvence je synchronizovaná s pozicí v taktu hostitele.
- Master bus: glue kompresor + saturace (2× oversampling).
- Přehrává i MIDI podle General MIDI drum mapy (tomy podle noty v různých výškách).
- Zvukové presety (Analog Classic, Deep Boom, Tight Techno, Punchy House, Hip Hop Knock, Industrial,
  Minimal Click, Trap Sub) a patterny (Four on the Floor, House, Techno, Breakbeat, Hip Hop, Trap,
  Electro, Disco, Polymeter).

### Sextant

FM syntezátor se šesti operátory.

- Každý operátor je jeden ze čtyř typů: **FM oscilátor, ring modulátor, filtr, wavefolder**.
- 18 přednastavených algoritmů + vlastní routování kreslené v matici.
- Operátor: vlna, fixní frekvence nebo poměr (coarse/fine/detune), level, feedback, velocity, key scaling, ADSR.
- Voice: Poly / Mono / Legato, unison (detune, spread), glide, pitch bend.
- Globální filtr (LP 12 / LP 24 / BP / HP) s obálkou, key trackingem a velocity.
- LFO (Sine / Triangle / Square / Saw / S&H) na pitch, FM, amplitudu a filtr; vibrato z modulačního kolečka.
- Efekty: chorus a ping-pong delay (volný nebo synchronizovaný s tempem).
- Hlasy běží na 2× oversamplingu, takže i vysoké FM indexy znějí čistě.

### Pollen

Granulární syntezátor pro plochy a textury.

- **Zdroj:** 5 vestavěných zvuků (Glass, Choir, Strings, Breath, Music Box) nebo vlastní sample –
  stačí přetáhnout soubor na waveform.
- **Grainy:** Position, Scan (posun playheadu), Spray, Drift, Size, Density, Shape (Hann … flat-top),
  Transpose, Jitter, Octaves, Reverse, Spread.
- Size a Density nemění hlasitost. Transpozice nahoru nealiasuje.
- Obálka Attack / Release, Tone (low-pass), vestavěný reverb (z Nimbusu) s Decay.
- 13 presetů: First Light, Dust Choir, Slow Tide, Wind Organ, Music Box Cloud, Frozen Bell, Shimmer Veil,
  Granite, Reverse Halo, Underwater, Glass Rain, Long Exhale, Choir Swell.

---

## Stažení a instalace

Zbuildované pluginy jsou ke stažení v sekci [Releases](https://github.com/UNVRSmusic/unvrs-plugins/releases).

Na macOS zkopíruj:
- `.vst3` do `~/Library/Audio/Plug-Ins/VST3`
- `.component` (AU) do `~/Library/Audio/Plug-Ins/Components`

Pluginy nejsou notarizované od Applu, takže je macOS po stažení může zablokovat. Odblokuješ je v Terminálu:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/*.vst3 ~/Library/Audio/Plug-Ins/Components/*.component
```

## Build ze zdrojáků (macOS)

Vyžaduje Xcode a CMake. JUCE 9.0.5 je jako git submodul ve složce `JUCE/`.

```sh
git clone --recursive https://github.com/UNVRSmusic/unvrs-plugins.git
cd unvrs-plugins
cmake -B build -G Xcode
cmake --build build --config Release
```

Po buildu se pluginy automaticky zkopírují do `~/Library/Audio/Plug-Ins/VST3` a `~/Library/Audio/Plug-Ins/Components`.
Samostatné aplikace jsou v `build/<Plugin>/<Plugin>_artefacts/Release/Standalone/`
(S2K Converter v `build/S2KConverter_artefacts/Release/Standalone/`).

## Licence

Copyright © 2026 UNVRS. Licencováno pod [GNU AGPLv3](LICENSE).
JUCE (`JUCE/`) má vlastní licenci, viz [JUCE/LICENSE.md](JUCE/LICENSE.md).
