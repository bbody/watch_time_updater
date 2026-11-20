// GC9A01 240x240 round SPI TFT, as a Wokwi custom chip.
//
// Written from the GC9A01 command set and the Wokwi Chips API
// (https://docs.wokwi.com/chips-api/getting-started).
// SPDX-License-Identifier: MIT (see LICENSE in the repository root)
//
// Modelled: 4-wire SPI (CS, DC, SCL, SDA), hardware reset (RST), SWRESET,
// CASET/RASET/RAMWR/RAMWRC, MADCTL (rotation, mirroring, RGB/BGR order),
// COLMOD (16-bit and 18-bit pixels) and INVON/INVOFF.
// Accepted but ignored: sleep, display on/off, gamma, power and the vendor
// init registers. Reads over MISO are not supported.
//
// Real GC9A01 modules show an upright, true-colour image when the host sends
// MADCTL = MX | BGR and INVON, which is what Adafruit_GC9A01A and TFT_eSPI do
// at rotation 0. This model uses that as its reference orientation and colour.
//
// Only one instance per simulation is supported (the state is static).
// The part attribute "round" (default 1) paints the corners outside the
// circular glass board green; set it to 0 to see the full 240x240 frame.

#include "wokwi-api.h"

#define WIDTH 240u
#define HEIGHT 240u
#define SPI_BUFFER_SIZE 1024u
#define RUN_MAX WIDTH

#define CMD_SWRESET 0x01
#define CMD_INVOFF 0x20
#define CMD_INVON 0x21
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_RAMWR 0x2C
#define CMD_MADCTL 0x36
#define CMD_COLMOD 0x3A
#define CMD_RAMWRC 0x3C

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_BGR 0x08

// Framebuffer pixels are RGBA bytes, i.e. 0xAABBGGRR as a little-endian word.
#define RGB_MASK 0x00FFFFFFu
#define OPAQUE 0xFF000000u
// Outside the round glass: circuit-board green (#1F6E43), so it's clear
// which pixels can't be drawn.
#define BOARD_GREEN (OPAQUE | 0x436E1Fu)

typedef struct {
  pin_t cs_pin;
  pin_t dc_pin;
  pin_t rst_pin;
  spi_dev_t spi;
  buffer_t framebuffer;
  bool round_mask;

  bool spi_active;
  bool spi_stopping;
  bool corners_painted_from_spi;
  bool data_mode;  // DC high: bytes are parameters or pixel data

  uint8_t command;
  uint8_t params[4];
  uint32_t param_count;

  uint16_t col_start, col_end;
  uint16_t row_start, row_end;
  uint16_t col, row;
  uint8_t madctl;
  uint8_t colmod;
  bool inverted;  // INVON is active

  uint8_t pixel[3];
  uint32_t pixel_len;

  // Consecutive framebuffer pixels, written with one buffer_write().
  uint32_t run[RUN_MAX];
  uint32_t run_len;
  uint32_t run_offset;

  uint8_t spi_buffer[SPI_BUFFER_SIZE];
} chip_state_t;

static chip_state_t chip;

static bool in_glass(uint32_t x, uint32_t y) {
  if (!chip.round_mask) {
    return true;
  }
  const int32_t dx = (int32_t)(2 * x + 1) - (int32_t)WIDTH;
  const int32_t dy = (int32_t)(2 * y + 1) - (int32_t)HEIGHT;
  return dx * dx + dy * dy <= (int32_t)(WIDTH * WIDTH);
}

static void flush_run(void) {
  if (chip.run_len > 0) {
    buffer_write(chip.framebuffer, chip.run_offset, chip.run, chip.run_len * 4);
    chip.run_len = 0;
  }
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t rgba) {
  const uint32_t offset = (y * WIDTH + x) * 4;
  if (chip.run_len > 0
      && (chip.run_len == RUN_MAX || offset != chip.run_offset + chip.run_len * 4)) {
    flush_run();
  }
  if (chip.run_len == 0) {
    chip.run_offset = offset;
  }
  chip.run[chip.run_len++] = rgba;
}

// Black glass, with the corners outside it in board green. Pixel writes and
// inversion only touch the glass, so the corners stay green.
static void clear_screen(void) {
  flush_run();
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      chip.run[x] = in_glass(x, y) ? OPAQUE : BOARD_GREEN;
    }
    buffer_write(chip.framebuffer, y * WIDTH * 4, chip.run, WIDTH * 4);
  }
}

// Repaints only the corners. Called again after startup (from a timer, on
// every reset and on the first SPI transfer) in case the screen didn't show
// what chip_init() drew.
static void paint_corners(void) {
  if (!chip.round_mask) {
    return;
  }
  flush_run();
  for (uint32_t y = 0; y < HEIGHT; y++) {
    for (uint32_t x = 0; x < WIDTH; x++) {
      if (!in_glass(x, y)) {
        put_pixel(x, y, BOARD_GREEN);
      }
    }
  }
  flush_run();
}

static void on_startup_timer(void *user_data) {
  (void)user_data;
  paint_corners();
}

// INVON/INVOFF change what the panel shows, so the whole screen is redrawn.
static void set_inversion(bool on) {
  if (on == chip.inverted) {
    return;
  }
  flush_run();
  for (uint32_t y = 0; y < HEIGHT; y++) {
    buffer_read(chip.framebuffer, y * WIDTH * 4, chip.run, WIDTH * 4);
    for (uint32_t x = 0; x < WIDTH; x++) {
      if (in_glass(x, y)) {
        chip.run[x] = (chip.run[x] ^ RGB_MASK) | OPAQUE;
      }
    }
    buffer_write(chip.framebuffer, y * WIDTH * 4, chip.run, WIDTH * 4);
  }
  chip.inverted = on;
}

static void reset_state(void) {
  flush_run();
  chip.command = 0;
  chip.param_count = 0;
  chip.pixel_len = 0;
  chip.col_start = 0;
  chip.col_end = WIDTH - 1;
  chip.row_start = 0;
  chip.row_end = HEIGHT - 1;
  chip.col = 0;
  chip.row = 0;
  chip.madctl = 0;
  chip.colmod = 0x66;  // assumed 18-bit until the host sends COLMOD
  set_inversion(false);
  paint_corners();
}

static void advance(void) {
  if (chip.col >= chip.col_end) {
    chip.col = chip.col_start;
    chip.row = chip.row >= chip.row_end ? chip.row_start : chip.row + 1;
  } else {
    chip.col++;
  }
}

static void write_pixel(uint32_t rgb) {
  uint32_t x = chip.col;
  uint32_t y = chip.row;
  if (x < WIDTH && y < HEIGHT) {
    if (chip.madctl & MADCTL_MV) {
      const uint32_t t = x;
      x = y;
      y = t;
    }
    // The panel is mirrored horizontally, so MX set means "not mirrored".
    if (!(chip.madctl & MADCTL_MX)) {
      x = WIDTH - 1 - x;
    }
    if (chip.madctl & MADCTL_MY) {
      y = HEIGHT - 1 - y;
    }
    if (in_glass(x, y)) {
      // The panel inverts natively, so INVON shows the colour as sent.
      const uint32_t shown = chip.inverted ? rgb : (~rgb & RGB_MASK);
      put_pixel(x, y, shown | OPAQUE);
    }
  }
  advance();
}

static uint32_t expand5(uint32_t v) { return (v << 3) | (v >> 2); }
static uint32_t expand6(uint32_t v) { return (v << 2) | (v >> 4); }

static void pixel_byte(uint8_t value) {
  const bool rgb666 = (chip.colmod & 0x07) == 0x06;
  chip.pixel[chip.pixel_len++] = value;
  if (chip.pixel_len < (rgb666 ? 3u : 2u)) {
    return;
  }
  chip.pixel_len = 0;

  uint32_t r, g, b;
  if (rgb666) {
    r = expand6(chip.pixel[0] >> 2);
    g = expand6(chip.pixel[1] >> 2);
    b = expand6(chip.pixel[2] >> 2);
  } else {
    const uint32_t hi = chip.pixel[0];
    const uint32_t lo = chip.pixel[1];
    r = expand5(hi >> 3);
    g = expand6(((hi & 0x07) << 3) | (lo >> 5));
    b = expand5(lo & 0x1F);
  }
  // The panel is BGR, so the BGR bit gives the colour order the host intended.
  if (!(chip.madctl & MADCTL_BGR)) {
    const uint32_t t = r;
    r = b;
    b = t;
  }
  write_pixel(r | (g << 8) | (b << 16));
}

static void handle_command(uint8_t command) {
  chip.command = command;
  chip.param_count = 0;
  chip.pixel_len = 0;
  switch (command) {
    case CMD_SWRESET:
      reset_state();
      break;
    case CMD_INVOFF:
      set_inversion(false);
      break;
    case CMD_INVON:
      set_inversion(true);
      break;
    case CMD_RAMWR:
      chip.col = chip.col_start;
      chip.row = chip.row_start;
      break;
    default:
      // RAMWRC carries on from where the last write stopped.
      // Everything else is accepted and ignored.
      break;
  }
}

static void handle_data(uint8_t value) {
  switch (chip.command) {
    case CMD_CASET:
    case CMD_RASET:
      if (chip.param_count < 4) {
        chip.params[chip.param_count++] = value;
        if (chip.param_count == 4) {
          const uint16_t start = (uint16_t)((chip.params[0] << 8) | chip.params[1]);
          const uint16_t end = (uint16_t)((chip.params[2] << 8) | chip.params[3]);
          if (chip.command == CMD_CASET) {
            chip.col_start = start;
            chip.col_end = end;
          } else {
            chip.row_start = start;
            chip.row_end = end;
          }
        }
      }
      break;
    case CMD_MADCTL:
      if (chip.param_count++ == 0) {
        chip.madctl = value;
      }
      break;
    case CMD_COLMOD:
      if (chip.param_count++ == 0) {
        chip.colmod = value;
      }
      break;
    case CMD_RAMWR:
    case CMD_RAMWRC:
      pixel_byte(value);
      break;
    default:
      break;
  }
}

static void spi_begin(void) {
  if (!chip.spi_active) {
    chip.spi_active = true;
    spi_start(chip.spi, chip.spi_buffer, SPI_BUFFER_SIZE);
  }
}

// Ends the current transfer; on_spi_done() handles the bytes received so far.
static void spi_end(void) {
  if (chip.spi_active) {
    chip.spi_stopping = true;
    spi_stop(chip.spi);
    chip.spi_stopping = false;
    chip.spi_active = false;
  }
}

static void on_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  (void)user_data;
  chip.spi_active = false;
  if (pin_read(chip.rst_pin) == HIGH) {
    for (uint32_t i = 0; i < count; i++) {
      if (chip.data_mode) {
        handle_data(buffer[i]);
      } else {
        handle_command(buffer[i]);
      }
    }
    flush_run();
    // Drawing from here is known to reach the screen, so paint the corners
    // once more on the first transfer.
    if (!chip.corners_painted_from_spi) {
      chip.corners_painted_from_spi = true;
      paint_corners();
    }
  }
  if (!chip.spi_stopping && pin_read(chip.cs_pin) == LOW) {
    spi_begin();
  }
}

static void on_pin_change(void *user_data, pin_t pin, uint32_t value) {
  (void)user_data;
  if (pin == chip.cs_pin) {
    if (value == LOW) {
      spi_begin();
    } else {
      spi_end();
    }
  } else if (pin == chip.dc_pin) {
    // Bytes already received belong to the old DC level.
    spi_end();
    chip.data_mode = value == HIGH;
    if (pin_read(chip.cs_pin) == LOW) {
      spi_begin();
    }
  } else if (pin == chip.rst_pin && value == LOW) {
    spi_end();
    reset_state();
    if (pin_read(chip.cs_pin) == LOW) {
      spi_begin();
    }
  }
}

void chip_init(void) {
  chip.cs_pin = pin_init("CS", INPUT);
  chip.dc_pin = pin_init("DC", INPUT);
  chip.rst_pin = pin_init("RST", INPUT_PULLUP);
  chip.round_mask = attr_read(attr_init("round", 1)) != 0;

  uint32_t width = 0;
  uint32_t height = 0;
  chip.framebuffer = framebuffer_init(&width, &height);  // 240x240 from gc9a01.chip.json
  (void)width;
  (void)height;

  const spi_config_t spi_config = {
    .user_data = &chip,
    .sck = pin_init("SCL", INPUT),
    .mosi = pin_init("SDA", INPUT),
    .miso = NO_PIN,
    .mode = 0,
    .done = on_spi_done,
  };
  chip.spi = spi_init(&spi_config);

  const pin_watch_config_t watch_config = {
    .user_data = &chip,
    .edge = BOTH,
    .pin_change = on_pin_change,
  };
  pin_watch(chip.cs_pin, &watch_config);
  pin_watch(chip.dc_pin, &watch_config);
  pin_watch(chip.rst_pin, &watch_config);

  clear_screen();
  reset_state();
  chip.data_mode = pin_read(chip.dc_pin) == HIGH;
  if (pin_read(chip.cs_pin) == LOW) {
    spi_begin();
  }

  const timer_config_t timer_config = {
    .user_data = &chip,
    .callback = on_startup_timer,
  };
  timer_start(timer_init(&timer_config), 1000, false);  // 1 ms after start
}
