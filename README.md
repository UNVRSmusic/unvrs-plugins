# S2K Converter

Plugin (VST3 / AU / Standalone) emulující signálovou cestu 16bitového rackového sampleru typu Akai S2000.

## Signálový řetězec

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

## Tipy

- **Autentický S2000:** 44,1 kHz, 16 bit, Linear, oba filtry zapnuté, Drive ~20 %.
- **Typický „sampled“ zvuk 22 kHz:** 22,05 kHz – zaoblené výšky a víc grain.
- **Lo-fi / špinavé:** vypnout AA FILTER a DAC FILTER, interpolace Drop, 11 kHz, 8–12 bit.
- **Přebuzení ADC:** INPUT nahoru, dokud nesvítí ADC CLIP (tvrdý digitální clip jako na hardwaru).

## Build (macOS)

Vyžaduje Xcode a CMake. JUCE 9.0.5 je naklonované ve složce `JUCE/`.

```sh
cd ~/code/s2k-converter
cmake -B build -G Xcode
cmake --build build --config Release
```

Po buildu se plugin automaticky zkopíruje do `~/Library/Audio/Plug-Ins/VST3` a `~/Library/Audio/Plug-Ins/Components`.
Samostatná aplikace je v `build/S2KConverter_artefacts/Release/Standalone/`.

## Omezení

- Je to model postavený na známé architektuře přístroje (16 bit, 44,1/22,05 kHz, interpolace engine,
  2pólový filtr, analogový výstup). Není to měření skutečného kusu, takže konkrétní charakteristiky
  filtrů a saturace jsou odhad.
- Ladění vzorku nahoru a dolů (transpozice při přehrávání) neemuluje, protože efekt v reálném čase nemůže měnit délku signálu.
- Změna parametru Sample Rate resetuje převodník, takže při přepnutí může krátce lupnout.
# unvrs-plugins

## Licence

Copyright © 2026 UNVRS. Licencováno pod [GNU AGPLv3](LICENSE).
JUCE (`JUCE/`) má vlastní licenci, viz [JUCE/LICENSE.md](JUCE/LICENSE.md).
