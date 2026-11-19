// JJY time-code (TCO) encoding adapted from BotanicFields BF-018A Rev.3
// https://github.com/botanicfields/BF-018A (BF-018ARev3/BF-018ARev3.ino)
// Copyright (c) 2020 BotanicFields, Inc. MIT License; see LICENSE in this directory.

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "JjyEncoder.h"
#include "../../time/TimeFormat.h"

namespace JjyEncoder {
    namespace {
        constexpr int MARKER = 0xff;

        struct tm td = {};
        struct timespec ts = {};

        CarrierOnFn carrierOn = nullptr;
        CarrierOffFn carrierOff = nullptr;

        int int3Bcd(int value) {
            return (value % 10)
                + (value / 10 % 10 * 16)
                + (value / 100 % 10 * 256);
        }

        int parity8(int value) {
            int parity = value;

            for (int i = 1; i < 8; i++) {
                parity += value >> i;
            }

            return parity % 2;
        }

        int tcoValue() {
            const int daysOfMonth[12] = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };

            const int bcdHour = int3Bcd(td.tm_hour);
            const int parityBcdHour = parity8(bcdHour);

            const int bcdMinute = int3Bcd(td.tm_min);
            const int parityBcdMinute = parity8(bcdMinute);

            const int year = td.tm_year + 1900;
            const int bcdYear = int3Bcd(year);

            int days = td.tm_mday;
            for (int i = 0; i < td.tm_mon; i++) {
                days += daysOfMonth[i];
            }

            if (
                td.tm_mon >= 2
                && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            ) {
                days++;
            }

            const int bcdDays = int3Bcd(days);
            const int dayOfWeek = td.tm_wday;

            switch (td.tm_sec) {
                case 0:
                    return MARKER;
                case 1:
                    return bcdMinute & 0x40;
                case 2:
                    return bcdMinute & 0x20;
                case 3:
                    return bcdMinute & 0x10;
                case 4:
                    return 0;
                case 5:
                    return bcdMinute & 0x08;
                case 6:
                    return bcdMinute & 0x04;
                case 7:
                    return bcdMinute & 0x02;
                case 8:
                    return bcdMinute & 0x01;
                case 9:
                    return MARKER;

                case 10:
                case 11:
                    return 0;
                case 12:
                    return bcdHour & 0x20;
                case 13:
                    return bcdHour & 0x10;
                case 14:
                    return 0;
                case 15:
                    return bcdHour & 0x08;
                case 16:
                    return bcdHour & 0x04;
                case 17:
                    return bcdHour & 0x02;
                case 18:
                    return bcdHour & 0x01;
                case 19:
                    return MARKER;

                case 20:
                case 21:
                    return 0;
                case 22:
                    return bcdDays & 0x200;
                case 23:
                    return bcdDays & 0x100;
                case 24:
                    return 0;
                case 25:
                    return bcdDays & 0x080;
                case 26:
                    return bcdDays & 0x040;
                case 27:
                    return bcdDays & 0x020;
                case 28:
                    return bcdDays & 0x010;
                case 29:
                    return MARKER;

                case 30:
                    return bcdDays & 0x008;
                case 31:
                    return bcdDays & 0x004;
                case 32:
                    return bcdDays & 0x002;
                case 33:
                    return bcdDays & 0x001;
                case 34:
                case 35:
                    return 0;
                case 36:
                    return parityBcdHour;
                case 37:
                    return parityBcdMinute;
                case 38:
                    return 0;
                case 39:
                    return MARKER;

                case 40:
                    return 0;
                case 41:
                    return bcdYear & 0x80;
                case 42:
                    return bcdYear & 0x40;
                case 43:
                    return bcdYear & 0x20;
                case 44:
                    return bcdYear & 0x10;
                case 45:
                    return bcdYear & 0x08;
                case 46:
                    return bcdYear & 0x04;
                case 47:
                    return bcdYear & 0x02;
                case 48:
                    return bcdYear & 0x01;
                case 49:
                    return MARKER;

                case 50:
                    return dayOfWeek & 0x04;
                case 51:
                    return dayOfWeek & 0x02;
                case 52:
                    return dayOfWeek & 0x01;
                case 53:
                case 54:
                case 55:
                case 56:
                case 57:
                case 58:
                    return 0;
                case 59:
                    return MARKER;
                default:
                    return 0;
            }
        }

        void tcOn() {
            if (carrierOn != nullptr) {
                carrierOn();
            }
        }

        void tcOff() {
            if (carrierOff != nullptr) {
                carrierOff();
            }
        }

        void tco000ms() {
            tcOn();
        }

        void tco200ms() {
            if (tcoValue() == MARKER) {
                tcOff();
            }
        }

        void tco500ms() {
            if (tcoValue() != 0) {
                tcOff();
            }
        }

        void tco800ms() {
            tcOff();

            if (tcoValue() == 0) {
                (void)0;
            }
        }
    }

    void setCarrierCallbacks(CarrierOnFn on, CarrierOffFn off) {
        carrierOn = on;
        carrierOff = off;
    }

    void onTicker() {
        struct timeval tv = {};
        gettimeofday(&tv, nullptr);

        if (!TimeFormat::utcTimevalToJstTm(tv, td)) {
            tcOff();
            return;
        }

        clock_gettime(CLOCK_REALTIME, &ts);
        const int ts100ms = static_cast<int>(ts.tv_nsec / 100000000);

        switch (ts100ms) {
            case 0:
                tco000ms();
                break;
            case 2:
                tco200ms();
                break;
            case 5:
                tco500ms();
                break;
            case 8:
                tco800ms();
                break;
            default:
                break;
        }
    }
}
