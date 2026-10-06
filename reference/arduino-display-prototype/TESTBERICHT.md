# Prüfbericht

## Umlautänderung

- Displaytexte auf „LAUTSTÄRKE“, „Zurück“ und „HALTEN: ZURÜCK“ umgestellt.
- UTF-8-Kodierung der Textdateien und vorhandene Glyphen für ÄÖÜäöüß in allen vier Textschriftgrößen geprüft.
- Textbreiten aus den eingebetteten Schriftmetriken geprüft: 95/156, 59/148 und 126/164 Pixel (Text/verfügbarer Platz).
- Keine Änderung an Schriftdateien, LVGL-Konfiguration oder Steuerungslogik.
- Für diese reine Textänderung kein erneuter ESP32-Build oder Hardwaretest durchgeführt.

Die folgenden Abschnitte dokumentieren die Prüfungen des vorherigen Funktionsstands.

## Neue Dienste: WLAN-Uhr, Preferences und PWM

Die tatsächlich ausgelieferten Module `network_time.cpp`, `settings.cpp` und `backlight.cpp`
werden zusammen mit der UI als C++11 mit `-Wall -Wextra -Werror` kompiliert und ausgeführt.
Getestet in zwei Builds: ohne SSID und mit einer ausschließlich im Test gesetzten SSID.
`SmartKnob.ino` wurde zusätzlich mit dem Host-Arduino-Header auf C++-Syntax geprüft.
Die verwendeten LEDC-/Preferences-Signaturen und `configTzTime` wurden mit offiziellen
Arduino-ESP32-3.3.7-Quellen abgeglichen. Das ersetzt keinen kompletten Cross-Build.

Bestanden:

- Fehlende SSID: keine WLAN-Versuche, Status `WLAN EINTRAGEN`, Uhr `--:--`.
- Konfiguriertes, nicht erreichbares WLAN: Wiederholungsversuch auch über `millis()`-Überlauf.
- Verbunden ohne Zeit: weiter `--:--`, keine erfundene Uhrzeit.
- 14.09.2026, 10:28 UTC: Anzeige `12:28`, `Mo, 14.09.2026` mit mitteleuropäischer Sommerzeit.
- Nach WLAN-Verlust bleibt die Uhr sichtbar und läuft bei fortschreitender Systemzeit weiter.
- Wiederverbinden startet SNTP nicht pro Frame erneut.
- Winterzeit sowie beide Umschaltgrenzen im März und Oktober 2026.
- PWM-Tastgrade für 10 %, 80 % und 100 %; Begrenzung auch bei direkten ungültigen API-Eingaben.
- Live-Helligkeit schreibt nicht in NVS; Abbrechen stellt vorherigen PWM-Wert wieder her.
- Speicherung, unveränderte Bestätigung ohne weiteren Schreibzugriff, Öffnungs-/Schreibfehler.
- Speicherfehler meldet sich sichtbar und stellt auch die ursprüngliche Helligkeit wieder her.
- Gespeicherte Ausreißer werden geklemmt, Temperatur auf gültige Halbgradschritte ausgerichtet.
- Erneutes Öffnen des Speicherdienstes und kompletter Neuaufbau der UI stellen alle fünf Werte wieder her.
- Standby dimmt auf 10 %, Aufwecken auf den gespeicherten Wert; PWM-Fade über den Zählerüberlauf.
- Fehlgeschlagene PWM-Initialisierung wird zurückgemeldet.
- Uhransichten mit und ohne Synchronisierung, Offline-Uhr und 10-%-Regler im runden Ausschnitt gerendert und visuell geprüft.

Wichtig: NVS wird im Host-Test durch einen Speicherersatz simuliert. Ein realer Stromausfall-/Neustarttest,
ein WLAN-/NTP-Netzwerktest und die Messung des PWM-Signals am ESP32 stehen noch aus.

## Prüfung der größeren Darstellung

Neue Schriftdatei `knob_fonts.c` als C kompiliert, UI und Eingabelogik als C++11 mit
`-Wall -Wextra -Werror` gegen Original-LVGL 8.3.11. Der Testheader enthält weiterhin die
kollidierenden Arduino-/ESP32-Bezeichnungen, damit der frühere Fehler abgedeckt bleibt.
Alle vorhandenen Bedien- und Speichertests bestehen mit der kräftigeren Oberfläche erneut.
Hauptseite, Musik, Temperatur, Endanschlag, Menü und Bestätigung wurden im runden
240×240-Ausschnitt visuell kontrolliert. Vorschau und Screenshots wurden neu gerendert.

## Regressionstest für den gemeldeten Compilerfehler

Aus dem Nutzerprotokoll: Arduino-ESP32 3.3.7, LVGL 8.3.11, Arduino_GFX 1.6.7.
Der Testheader enthält jetzt zusätzlich `PI`, `DISPLAY` und einen globalen `STATUS`-Typ,
entsprechend den im Protokoll sichtbaren Namenskollisionen.
Der vorherige UI-Code schlägt damit beim Kompilieren reproduzierbar fehl.
Nach der Umbenennung kompiliert der ausgelieferte Test mit `-Wall -Wextra -Werror`;
alle unten genannten Bedien- und Speichertests bestehen erneut.
Dies ist ein gezielter Regressionstest, kein vollständiger ESP32-Cross-Build.

## Durchgeführt

UI und Tasterauswertung wurden mit `g++ -std=c++11 -Wall -Wextra -Werror`
gegen die **Originalquellen von LVGL 8.3.11** kompiliert und ausgeführt.
Arduino-Zeit/GPIO sowie die ESP32-Schnittstellen für WLAN, NVS und PWM werden im Desktop-Test simuliert.
LVGL-Objekte, Animationen, Fonts, Arc, Speicherverwaltung und RGB565-Rendering sind echt.

Erfolgreich geprüft:

- Tasterprellen, Wiederholung, kurzer Druck, langer Druck ohne anschließenden falschen Kurzklick.
- Taster-Zeitmessung über den 32-Bit-Zählerüberlauf.
- 101 schnelle Schritte rechts und 100 links mit laufender Animation: korrekte Auswahl, Animation erreicht Ziel.
- Werte bleiben bei 0/100 und Temperatur-Schritten von 0,5 innerhalb ihrer Grenzen.
- Sofort sichtbarer Zielwert und weich nachlaufender Ring.
- Übernehmen und tatsächliches Verwerfen eines noch animierten Entwurfs.
- Menüauswahl, „Zurueck“, zeitgesteuerte Statusmeldung, Standby und Verbrauch der Aufweckgeste.
- Automatische Rückkehr und Inaktivitätsprüfung über den 32-Bit-Zählerüberlauf.
- 300 schnelle Bearbeiten-/Abbrechen-Zyklen: freier LVGL-Pool davor und danach **73.000 Bytes**; kein wachsender Speicherverbrauch im Test.
- `lv_mem_test()` erfolgreich.
- 134 gerenderte Animationsframes und sieben Screenshots; sichtbare Hauptansichten auf dem runden Ausschnitt geprüft.

## Grenzen

Kein abgeschlossener Arduino-/Xtensa-Cross-Build: Der Boardverwalter konnte seine Toolchain nicht herunterladen.
`display.cpp` und der Arduino-Startcode wurden geprüft, aber nicht mit der ESP32-Toolchain gebaut.
Kein angeschlossenes ESP32-S3-Modul, keine Messung von Display-SPI, Backlight oder echter Bildrate.
Insbesondere ist die neue Netzwerk-/PWM-Funktion noch nicht auf deiner Hardware verifiziert.
Der Desktop-Test bestätigt die LVGL-UI und Eingabelogik, nicht die elektrische Verdrahtung oder Hardwareleistung.
Die GIF-Zeitbasis ist eine Simulationsaufzeichnung mit 40 ms je Frame, kein Leistungsbenchmark.

## Reproduzieren

Voraussetzungen: Python 3, GCC/G++, `ar` und entpackte [LVGL-8.3.11-Quellen](https://github.com/lvgl/lvgl/releases/tag/v8.3.11).
Im Ordner `SmartKnob/tests`:

```sh
python3 build_host.py /absoluter/pfad/zu/lvgl-8.3.11
mkdir -p frames
./check_ui
./check_ui_wifi
```

Der Test schreibt PPM-Screenshots und Animationsframes in das aktuelle Verzeichnis.
Er verwendet keine zusätzlichen Test-Frameworks. Die Arduino-Bibliotheken gehören nicht in diesen Testordner.
