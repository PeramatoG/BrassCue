// BrassCue - GPL-3.0. Read typed text before consulting the spin control's cached value.
#pragma once
#include <cmath>
#include <stdexcept>
#include <wx/numformatter.h>
#include <wx/spinctrl.h>
inline double ReadFadeSeconds(const wxSpinCtrlDouble &field) {
    double value = 0;
    if (!wxNumberFormatter::FromString(field.GetTextValue(), &value) || !std::isfinite(value) ||
        value < field.GetMin() || value > field.GetMax())
        throw std::invalid_argument("Fade time must be a number from 0 to 60 seconds");
    return value;
}
