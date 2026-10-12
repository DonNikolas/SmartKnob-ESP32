# Press feedback test

Date: 2026-10-12

## Tested settings
- 12 detents, spaced 30 degrees apart.
- Spring voltage limit: 1.00 V.
- Spring stiffness: 3.00 V/rad.
- Requested click voltage: 3.00 V.
- Click duration: 5 ms in each direction.
- Pressure threshold: 15000 ADC counts.
- Release threshold: 7500 ADC counts.

## Result
I built and uploaded the firmware to my PCB.
The detents and pressure feedback felt satisfactory with these settings.

## Limitations
Weak-field warnings and tracking errors remain.
Tracking loss can cancel a click.
The requested click voltage is not a measured winding voltage.
Current draw, temperature and long-term reliability are not verified.

## AI assistance
ChatGPT helped create and revise the code using the earlier tests
and the Scott Bezek and SeedLabs implementations as references.
I tested the firmware and adjusted the settings.