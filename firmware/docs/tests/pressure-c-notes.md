# Pressure logic in C

This is a behaviour-preserving refactor, not a new pressure algorithm.
The original thresholds, startup, filter, diagnostic strings and event timing
are retained. Generated with ChatGPT assistance from the supplied pressure.h.

## Aufbau verstehen

- `src/app_gui/pressure_logic.h` beschreibt den Zustand (`PressureState`),
  Ereignisse und Funktionsdeklarationen. Es ist eine C-Schnittstelle.
- `src/app_gui/pressure_logic.c` enthaelt Nullpunktmessung, Mittelwertfilter,
  Fehlerpruefung und die Erkennung von kurzem/langem Druck.
- `src/app_gui/pressure.h` ist nur der Adapter zur C++-Bibliothek HX711 und zu
  Arduino `millis()`. Er haelt die bestehende Schnittstelle zur GUI bei.

Die C-Datei erhaelt ihre Eingaben als Parameter. Sie greift nicht selbst auf
Pins, Motor, Display oder Arduino zu. Deshalb laesst sie sich auch am PC testen.

## Was bedeutet s->pressed?

`PressureState` ist ein struct mit dem gesamten aktuellen Zustand. `s` ist ein
Zeiger auf diesen struct. `s->pressed` greift auf sein Feld `pressed` zu.
Das entspricht `(*s).pressed` in C. Wir verwenden einen Zustand pro Sensor.

## Ablauf eines Messwerts

1. Der Adapter prueft den Nullpunkt-Timeout und ob der HX711 bereit ist.
2. Er liest den Rohwert und uebergibt ihn mit Zeitstempeln an `pressure_poll`.
3. Die C-Logik prueft Sattigung und gegebenenfalls die Nullpunktmessung.
4. Nach einem haptischen Klick werden 150 ms ausgeblendet, wie bisher.
5. Vier Rohwerte bilden einen gleitenden Mittelwert; der Nullpunkt wird abgezogen.
6. `pressure_sample_gesture` entscheidet ueber Down, Short oder Long.
7. Der Adapter gibt das Ereignis an die unveraenderte GUI weiter.

## Bedingungen der bestehenden Logik

- Nullpunkt aus 32 Werten; deren Spannweite darf hoechstens 5000 Counts betragen.
- Rohwert UND gefilterter Wert muessen mindestens +40000 oder hoechstens -40000
  fuer 50 ms erreichen: `Down`. Die GUI kann damit den haptischen Klick starten.
- Nach bestaetigtem Druck wird dessen Richtung festgehalten.
- Loslassen unter 10000 in dieser Richtung fuer 60 ms: `Short`, falls noch kein
  `Long` gesendet wurde. Der kurze Druck wird also weiterhin beim Loslassen gemeldet.
- Gehalten fuer 900 ms und ausreichender Restdruck: einmal `Long`.
- Vor dem naechsten Druck muessen beide Betraege 150 ms unter 10000 liegen.
- Die Freigabe durch die Dreh-/GUI-Logik wird weiterhin beachtet.

Counts sind ADC-Zaehlwerte, keine Newton. Mit dieser Umstellung aendern wir
weder die mechanische Empfindlichkeit noch das Drueckgefuehl.

## Warum noch C++?

HX711 und der Arduino-Einstieg bleiben C++. `extern "C"` im C-Header sorgt
lediglich dafuer, dass C++ die vom C-Compiler erzeugten Funktionen korrekt
verlinken kann. Es macht die C-Datei nicht zu C++.
Der Adapter erbt den C-Zustand, damit die vorhandenen Diagnosezugriffe in der
GUI nicht alle umgeschrieben werden muessen. Die Entscheidungen liegen in C.

## Vergleichstest

`pressure-original.h` ist eine eingefrorene Kopie der alten Implementation,
mit geaendertem Namensraum, ausschliesslich fuer den Host-Test. Sie wird nicht
in die Firmware eingebaut. Die Stubs ersetzen Arduino und HX711 fuer diesen Test.

Aus dem firmware-Verzeichnis unter Linux:

```sh
gcc -std=c11 -Wall -Wextra -Werror -c src/app_gui/pressure_logic.c -o /tmp/pressure-logic.o
g++ -std=c++11 -Wall -Wextra -Werror -Idocs/tests/pressure-c-stubs docs/tests/pressure-c-host.cpp /tmp/pressure-logic.o -o /tmp/pressure-c-host
/tmp/pressure-c-host
```

Ergebnis: 240000 identische poll-Ergebnisse und oeffentliche Zustaende.
907 Down-, 395 Short- und 512 Long-Ereignisse. Geprueft wurden beide Richtungen,
Gegenschwingen, Startblockierung, Klick-Ausblendung, Filter, ADC-Sattigung,
fehlende Daten, Nullpunkt-Timeout und millis-Ueberlauf. Hardware und ein
vollstaendiger ESP32-Build bleiben separat zu pruefen. Die Testdaten sind
synthetisch, keine neue Messreihe des echten Knobs.
