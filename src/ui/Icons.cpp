#include <Arduino.h>
#include "Icons.h"

namespace Icons {
    namespace {
        int16_t sc(int16_t value, float scale) {
            return (int16_t)(value * scale + (value >= 0 ? 0.5f : -0.5f));
        }

        void drawSignalArc(
            Adafruit_GFX& tft,
            int16_t cx,
            int16_t anchorY,
            int16_t radius,
            uint16_t color,
            float scale
        ) {
            constexpr int START_DEG = 25;
            constexpr int END_DEG = 155;
            const int thicknessSteps = sc(4, scale);

            for (int deg = START_DEG; deg <= END_DEG; deg++) {
                const float rad = deg * DEG_TO_RAD;

                for (int thickness = 0; thickness < thicknessSteps; thickness++) {
                    const int r = radius + thickness;
                    const int x = cx + (int)(cosf(rad) * r);
                    const int y = anchorY - (int)(sinf(rad) * r);
                    tft.drawPixel(x, y, color);
                }
            }
        }
    }

    void drawWiFi(
        Adafruit_GFX& tft,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t anchorY = centerY + sc(22, scale);

        tft.fillCircle(centerX, anchorY, sc(6, scale), color);
        drawSignalArc(tft, centerX, anchorY, sc(20, scale), color, scale);
        drawSignalArc(tft, centerX, anchorY, sc(36, scale), color, scale);
        drawSignalArc(tft, centerX, anchorY, sc(52, scale), color, scale);
    }

    void drawGps(
        Adafruit_GFX& tft,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t pinR = sc(22, scale);
        const int16_t headY = centerY - sc(14, scale);

        tft.fillCircle(centerX, headY, pinR, color);
        tft.fillTriangle(
            centerX - pinR + sc(2, scale),
            centerY - sc(2, scale),
            centerX + pinR - sc(2, scale),
            centerY - sc(2, scale),
            centerX,
            centerY + sc(38, scale),
            color
        );

        tft.fillCircle(centerX, headY, sc(10, scale), 0x0000);
        tft.drawCircle(centerX, headY, sc(10, scale), color);
        tft.fillCircle(centerX, headY, sc(4, scale), color);
    }

    void drawEthernet(
        Adafruit_GFX& tft,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t portW = sc(58, scale);
        const int16_t portH = sc(46, scale);
        const int16_t cableW = sc(22, scale);
        const int16_t cableH = sc(16, scale);

        const int16_t portX = centerX - portW / 2;
        const int16_t portY = centerY - portH / 2 - sc(6, scale);

        tft.fillRoundRect(portX, portY, portW, portH, sc(8, scale), color);

        const int16_t insetX = portX + sc(10, scale);
        const int16_t insetY = portY + sc(10, scale);
        const int16_t insetW = portW - sc(20, scale);
        const int16_t insetH = portH - sc(18, scale);

        tft.fillRoundRect(insetX, insetY, insetW, insetH, sc(4, scale), 0x0000);

        for (int i = 0; i < 4; i++) {
            const int16_t pinX = insetX + sc(6, scale) + i * sc(10, scale);
            tft.fillRect(pinX, insetY + sc(6, scale), sc(5, scale), sc(7, scale), color);
            tft.fillRect(
                pinX,
                insetY + insetH - sc(13, scale),
                sc(5, scale),
                sc(7, scale),
                color
            );
        }

        const int16_t cableX = centerX - cableW / 2;
        const int16_t cableY = portY + portH - sc(2, scale);

        tft.fillRect(cableX, cableY, cableW, cableH, color);
        tft.fillCircle(centerX, cableY + cableH + sc(4, scale), sc(5, scale), color);
    }

    void drawChevronLeft(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t w = sc(8, scale);
        const int16_t h = sc(12, scale);

        gfx.fillTriangle(
            centerX - w,
            centerY,
            centerX + w / 2,
            centerY - h,
            centerX + w / 2,
            centerY + h,
            color
        );
    }

    void drawChevronRight(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t w = sc(8, scale);
        const int16_t h = sc(12, scale);

        gfx.fillTriangle(
            centerX + w,
            centerY,
            centerX - w / 2,
            centerY - h,
            centerX - w / 2,
            centerY + h,
            color
        );
    }

    void drawEnterMark(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t r = sc(3, scale);

        gfx.fillCircle(centerX - sc(5, scale), centerY, r, color);
        gfx.fillRect(
            centerX - sc(2, scale),
            centerY - sc(2, scale),
            sc(10, scale),
            sc(4, scale),
            color
        );
        gfx.fillTriangle(
            centerX + sc(10, scale),
            centerY,
            centerX + sc(3, scale),
            centerY - sc(6, scale),
            centerX + sc(3, scale),
            centerY + sc(6, scale),
            color
        );
    }

    void drawGear(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t outerR = sc(28, scale);
        const int16_t toothW = sc(10, scale);

        for (int i = 0; i < 8; i++) {
            const float rad = i * 45.0f * DEG_TO_RAD;
            const int16_t cx =
                centerX + static_cast<int16_t>(cosf(rad) * outerR * 0.55f);
            const int16_t cy =
                centerY + static_cast<int16_t>(sinf(rad) * outerR * 0.55f);
            gfx.fillRect(
                cx - toothW / 2,
                cy - sc(6, scale),
                toothW,
                sc(12, scale),
                color
            );
        }

        gfx.fillCircle(centerX, centerY, sc(8, scale), color);
        gfx.fillCircle(centerX, centerY, sc(4, scale), 0x0000);
    }

    void drawInfoMark(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        gfx.fillCircle(centerX, centerY - sc(14, scale), sc(4, scale), color);
        gfx.fillRect(
            centerX - sc(3, scale),
            centerY - sc(6, scale),
            sc(6, scale),
            sc(22, scale),
            color
        );
    }

    void drawBackArrow(
        Adafruit_GFX& gfx,
        int16_t centerX,
        int16_t centerY,
        uint16_t color,
        float scale
    ) {
        const int16_t stemW = sc(22, scale);
        const int16_t stemH = sc(6, scale);

        gfx.fillRect(
            centerX - stemW / 2 + sc(6, scale),
            centerY - stemH / 2,
            stemW,
            stemH,
            color
        );
        drawChevronLeft(gfx, centerX - sc(4, scale), centerY, color, scale);
    }
}
