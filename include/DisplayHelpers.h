#ifndef DISPLAY_HELPERS_H
#define DISPLAY_HELPERS_H

#include "GLOBAL_DEFINES.h"
#include "TFTs.h"
#include "Clock.h"

void updateClockDisplay(TFTs &tfts, Clock &uclock, TFTs::show_t show = TFTs::yes);

#endif
