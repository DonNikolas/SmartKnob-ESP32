# SmartKnob Display Concept

Date: 2026-10-06
Status: Planned. Not yet implemented or tested on the current PCB.

## Starting point

The existing SmartKnob_Umlaute Arduino sketch is the visual reference.
Its general layout will be kept and adapted to the current PCB.
All display text will be in English.

## Display and appearance

- Round GC9A01 display with 240 x 240 pixels.
- LVGL for the user interface.
- Dark background with bright text and coloured accents.
- Large symbols with thick lines and rounded edges.
- Main values: 52 px.
- Screen titles: 24 px.
- Important hints: preferably 16 px.
- Smaller text only where needed and checked on the actual display.

## Colours

| Element | Colour |
| --- | --- |
| Background | #080C10 |
| Cards | #141C23 |
| Inactive arc | #27323B |
| Main text | #EDF4EF |
| Secondary text | #84949F |
| Timer | #D5B2FF |
| Music | #AEF582 |
| Light | #FFCD7C |
| Temperature | #86C5FF |
| Settings | #B9ACFF |
| More | #C3D0D8 |

## Home screen

A horizontal carousel shows the available functions:
Timer, Music, Light, Temperature, Settings and More.

The selected function has a large central symbol.
Neighbouring functions appear smaller.
The function name and current value appear below the symbol.

## Value screen

- Large value in the centre with its unit.
- A coloured 270-degree arc shows the selected value.
- Minimum and maximum values are visible.
- Temperature settings are labelled "Target temperature".
- Measured temperature will be shown separately if added later.

Timer, Music, Light and Temperature initially represent local settings.
Actual device control will be added and tested separately.

## Navigation

- Turn on the home screen: select a function.
- Short press: open the selected function.
- Turn on the value screen: change the value.
- Short press: confirm and save.
- Long press on the value screen: cancel and return.
- Show a short confirmation or error message after saving.
- Show basic operating hints briefly when entering a screen.

Motor and press-sensor integration will be implemented separately.

## Animation

- Keep the existing carousel movement.
- Use short transitions of about 110 to 180 ms.
- Update the numeric value immediately.
- Animate the arc smoothly towards the new value.
- Keep the interface responsive during fast input.

## Brightness and standby

- Display brightness adjustable from 10% to 100%.
- Default brightness: 80%.
- Save the confirmed brightness setting.
- Enter standby after 20 seconds without input on the home screen.
- Show a 24-hour clock, date and Wi-Fi status in standby.
- Synchronise the clock over Wi-Fi.
- Show "--:--" until a valid time is available.
- Standby brightness: 10%.
- Fade brightness changes over about 300 ms.
- The first turn or press wakes the display without changing a value.

## LED ring: later integration

- Match the selected function's accent colour.
- Optionally indicate the current value.
- Keep the LED ring off in standby.

## First display test

Before implementing the complete interface, create a separate test:

- Red, green, blue, white and black colour screens.
- Text samples at the planned font sizes.
- A centred circle and orientation labels.
- An animated arc.
- Backlight adjustment from 10% to 100%.

Check colours, orientation, readability, clipping, animation and brightness.
Record the results and add photos before merging the test.

The old sketch's pin assignments must be replaced with the current PCB pins.

## Previous work and AI assistance

The visual design builds on an earlier display prototype developed
with ChatGPT assistance. This concept was reviewed and adapted for
the current PCB. Implementation and hardware tests will be documented
in this repository.