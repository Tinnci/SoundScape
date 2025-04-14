#ifndef TEMP_HUM_SCREEN_H
#define TEMP_HUM_SCREEN_H

#include "screen.h"

// Forward declaration
class DataManager;

class TempHumScreen : public Screen {
public:
    // Constructor now takes DataManager reference
    TempHumScreen(TFT_eSPI& display, DataManager& dataMgr);

    void draw(int yOffset = 0) override;
};

#endif // TEMP_HUM_SCREEN_H
