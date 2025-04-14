#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

#include "screen.h"

// Forward declaration
class DataManager;

class StatusScreen : public Screen {
public:
    // Constructor now takes DataManager reference
    StatusScreen(TFT_eSPI& display, DataManager& dataMgr);

    void draw(int yOffset = 0) override;
};

#endif // STATUS_SCREEN_H
