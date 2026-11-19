#pragma once

#include <Adafruit_GFX.h>

namespace Icons {
    void drawWiFi(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );
    void drawGps(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );
    void drawEthernet(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );

    void drawChevronLeft(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );

    void drawChevronRight(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );

    void drawEnterMark(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );

    void drawGear(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );

    void drawInfoMark(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );

    void drawBackArrow(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale = 1.0f
    );
}
