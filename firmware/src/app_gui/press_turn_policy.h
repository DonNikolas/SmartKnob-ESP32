#pragma once
#include <math.h>
namespace KnobPressTurn {
// Home has 8 detents per turn: accept within +/-18 degrees of its centre.
// Outer strip near each app boundary rejects NEW squeezes only.
inline bool allowStart(bool guard, bool home, float detentFraction) {
    return !guard || !home || fabsf(detentFraction) <= 0.40f;
}
inline bool lockSelection(bool pressed, bool consumed) { return pressed || consumed; }
}
