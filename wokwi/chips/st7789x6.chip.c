// Wokwi custom chip: six ST7789 controllers with 135x240 IPS panels on one SPI bus,
// as on the EleksTube style clocks (write-only 4-wire SPI, one CS line per panel).
//
// Why one chip for six panels: the firmware selects several panels at once (broadcast
// init / fillScreen) and Wokwi does not cope with several custom chips receiving on the
// same SPI bus at the same time. One chip with one SPI receiver avoids that and is faster.
//
// Emulated per panel:
//   - 240x320 GRAM, the panel shows a 135x240 window of it (attributes colOffset/rowOffset);
//     only that window is stored, writes outside of it are dropped
//   - SWRESET, SLPIN/SLPOUT, INVOFF/INVON, DISPOFF/DISPON, CASET, RASET, RAMWR, RAMWRC,
//     MADCTL (MY/MX/MV/BGR), COLMOD (16 bit RGB565 only); all other commands are ignored
// Shared: hardware reset (RES low), panel power (EN, active high), D/C, SPI.
// Not emulated: reading from the controller, 12/18 bit colour modes, scrolling, partial mode.
//
// Build: python wokwi/build_chip.py  ->  wokwi/chips/st7789x6.chip.wasm

#include "wokwi-api.h"
#include <stdint.h>
#include <stdbool.h>

#define NUM_PANELS 6
#define PANEL_W 135
#define PANEL_H 240
#define GRAM_W 240
#define GRAM_H 320
#define SPI_BUF_SIZE 4096

#define CMD_SWRESET 0x01
#define CMD_SLPIN 0x10
#define CMD_SLPOUT 0x11
#define CMD_INVOFF 0x20
#define CMD_INVON 0x21
#define CMD_DISPOFF 0x28
#define CMD_DISPON 0x29
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_RAMWR 0x2C
#define CMD_MADCTL 0x36
#define CMD_RAMWRC 0x3C

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_BGR 0x08

typedef struct
{
  // Controller state
  uint8_t cmd;
  uint8_t param_idx;
  uint8_t params[4];
  uint16_t xs, xe, ys, ye;
  uint16_t x, y;
  uint8_t madctl;
  bool inversion;
  bool display_on;
  bool sleeping;
  bool have_high_byte;
  uint8_t high_byte;
  // Visible window of the GRAM (RGB565) and dirty row range (-1 = clean)
  uint16_t vram[PANEL_W * PANEL_H];
  int16_t dirty_min;
  int16_t dirty_max;
} panel_t;

typedef struct
{
  pin_t pin_cs[NUM_PANELS];
  pin_t pin_dc;
  pin_t pin_rst;
  pin_t pin_en;
  spi_dev_t spi;
  bool spi_active;
  bool stopping;     // inside spi_stop(): spi_done() must not restart the transfer
  uint32_t dc_level; // D/C level that belongs to the bytes currently in the SPI buffer
  uint8_t selected;  // bit mask of the panels selected for the bytes in the SPI buffer
  uint8_t spi_buf[SPI_BUF_SIZE];

  buffer_t framebuffer;
  uint32_t fb_w;
  uint32_t fb_h;
  uint32_t gap; // dark pixels between two panels
  uint32_t col_offset;
  uint32_t row_offset;
  bool panel_inverted; // IPS panels show correct colours only with INVON

  panel_t panels[NUM_PANELS];
  uint32_t row_rgba[PANEL_W]; // conversion buffer for buffer_write()
} chip_state_t;

static chip_state_t chip; // static storage, zero-initialised

static uint32_t to_rgba(const chip_state_t *c, const panel_t *p, uint16_t px, bool powered)
{
  if (!powered || !p->display_on || p->sleeping)
    return 0xFF000000; // black, opaque
  if (p->inversion != c->panel_inverted)
    px = ~px;
  uint32_t r = (px >> 11) & 0x1F;
  uint32_t g = (px >> 5) & 0x3F;
  uint32_t b = px & 0x1F;
  r = (r << 3) | (r >> 2);
  g = (g << 2) | (g >> 4);
  b = (b << 3) | (b >> 2);
  if (p->madctl & MADCTL_BGR)
  {
    uint32_t t = r;
    r = b;
    b = t;
  }
  return 0xFF000000 | (b << 16) | (g << 8) | r; // RGBA in memory order
}

static void flush_panel(chip_state_t *c, uint32_t idx, bool powered)
{
  panel_t *p = &c->panels[idx];
  if (p->dirty_min < 0)
    return;
  uint32_t x0 = idx * (PANEL_W + c->gap);
  for (int32_t row = p->dirty_min; row <= p->dirty_max; row++)
  {
    const uint16_t *src = &p->vram[row * PANEL_W];
    for (uint32_t i = 0; i < PANEL_W; i++)
      c->row_rgba[i] = to_rgba(c, p, src[i], powered);
    buffer_write(c->framebuffer, ((uint32_t)row * c->fb_w + x0) * 4, c->row_rgba, PANEL_W * 4);
  }
  p->dirty_min = -1;
  p->dirty_max = -1;
}

static void flush_all(chip_state_t *c)
{
  bool powered = pin_read(c->pin_en) == HIGH; // one pin read per flush, not per pixel
  for (uint32_t i = 0; i < NUM_PANELS; i++)
    flush_panel(c, i, powered);
}

static void invalidate(panel_t *p)
{
  p->dirty_min = 0;
  p->dirty_max = PANEL_H - 1;
}

static void reset_panel(panel_t *p)
{
  p->cmd = 0;
  p->param_idx = 0;
  p->xs = 0;
  p->xe = GRAM_W - 1;
  p->ys = 0;
  p->ye = GRAM_H - 1;
  p->x = 0;
  p->y = 0;
  p->madctl = 0;
  p->inversion = false;
  p->display_on = false;
  p->sleeping = true;
  p->have_high_byte = false;
  invalidate(p);
}

static void write_pixel(chip_state_t *c, panel_t *p, uint16_t px)
{
  uint32_t pc, pr;
  // Logical to physical address mapping of the ST7789 (MADCTL MV/MX/MY)
  if (p->madctl & MADCTL_MV)
  {
    pc = p->y;
    pr = p->x;
  }
  else
  {
    pc = p->x;
    pr = p->y;
  }
  if (p->madctl & MADCTL_MX)
    pc = GRAM_W - 1 - pc;
  if (p->madctl & MADCTL_MY)
    pr = GRAM_H - 1 - pr;

  if (pc >= c->col_offset && pc < c->col_offset + PANEL_W && pr >= c->row_offset && pr < c->row_offset + PANEL_H)
  {
    int16_t fy = (int16_t)(pr - c->row_offset);
    p->vram[fy * PANEL_W + (pc - c->col_offset)] = px;
    if (p->dirty_min < 0 || fy < p->dirty_min)
      p->dirty_min = fy;
    if (fy > p->dirty_max)
      p->dirty_max = fy;
  }

  // Advance the address counter inside the window
  if (++p->x > p->xe)
  {
    p->x = p->xs;
    if (++p->y > p->ye)
      p->y = p->ys;
  }
}

static void handle_command(panel_t *p, uint8_t cmd)
{
  p->cmd = cmd;
  p->param_idx = 0;
  p->have_high_byte = false;
  switch (cmd)
  {
  case CMD_SWRESET:
    reset_panel(p);
    break;
  case CMD_SLPIN:
  case CMD_SLPOUT:
    p->sleeping = (cmd == CMD_SLPIN);
    invalidate(p);
    break;
  case CMD_INVOFF:
  case CMD_INVON:
    p->inversion = (cmd == CMD_INVON);
    invalidate(p);
    break;
  case CMD_DISPOFF:
  case CMD_DISPON:
    p->display_on = (cmd == CMD_DISPON);
    invalidate(p);
    break;
  case CMD_RAMWR:
    p->x = p->xs;
    p->y = p->ys;
    break;
  default:
    break;
  }
}

static void handle_data(chip_state_t *c, panel_t *p, uint8_t value)
{
  switch (p->cmd)
  {
  case CMD_RAMWR:
  case CMD_RAMWRC:
    if (!p->have_high_byte)
    {
      p->high_byte = value;
      p->have_high_byte = true;
    }
    else
    {
      p->have_high_byte = false;
      write_pixel(c, p, (uint16_t)((p->high_byte << 8) | value));
    }
    break;
  case CMD_CASET:
  case CMD_RASET:
    if (p->param_idx < 4)
      p->params[p->param_idx++] = value;
    if (p->param_idx == 4)
    {
      uint16_t start = (uint16_t)((p->params[0] << 8) | p->params[1]);
      uint16_t end = (uint16_t)((p->params[2] << 8) | p->params[3]);
      if (p->cmd == CMD_CASET)
      {
        p->xs = start;
        p->xe = end;
      }
      else
      {
        p->ys = start;
        p->ye = end;
      }
      p->param_idx = 0xFF; // ignore further bytes
    }
    break;
  case CMD_MADCTL:
    if (p->param_idx == 0)
    {
      p->madctl = value;
      p->param_idx = 1;
      invalidate(p); // BGR bit changes the colours of the visible content
    }
    break;
  default:
    break; // COLMOD and all other parameters: ignored (16 bit mode assumed)
  }
}

static uint8_t read_selection(chip_state_t *c)
{
  uint8_t mask = 0;
  for (uint32_t i = 0; i < NUM_PANELS; i++)
    if (pin_read(c->pin_cs[i]) == LOW)
      mask |= (uint8_t)(1u << i);
  return mask;
}

static void start_spi_if_selected(chip_state_t *c)
{
  if (!c->spi_active && c->selected && pin_read(c->pin_rst) == HIGH)
  {
    c->spi_active = true;
    spi_start(c->spi, c->spi_buf, SPI_BUF_SIZE);
  }
}

static void spi_done(void *user_data, uint8_t *buffer, uint32_t count)
{
  chip_state_t *c = (chip_state_t *)user_data;
  c->spi_active = false;
  for (uint32_t i = 0; i < NUM_PANELS; i++)
  {
    if (!(c->selected & (1u << i)))
      continue;
    panel_t *p = &c->panels[i];
    if (c->dc_level == LOW)
    {
      for (uint32_t n = 0; n < count; n++)
        handle_command(p, buffer[n]);
    }
    else
    {
      for (uint32_t n = 0; n < count; n++)
        handle_data(c, p, buffer[n]);
    }
  }
  flush_all(c);
  // Buffer full: continue receiving. After spi_stop() the caller restarts the
  // transfer itself - never start a new transfer from inside spi_stop().
  if (!c->stopping)
    start_spi_if_selected(c);
}

// Ends the running transfer; spi_done() processes the bytes received so far.
static void stop_spi(chip_state_t *c)
{
  if (!c->spi_active)
    return;
  c->stopping = true;
  spi_stop(c->spi);
  c->stopping = false;
  c->spi_active = false;
}

static void on_cs_change(void *user_data, pin_t pin, uint32_t value)
{
  chip_state_t *c = (chip_state_t *)user_data;
  stop_spi(c); // bytes so far belong to the previous selection
  c->selected = read_selection(c);
  c->dc_level = pin_read(c->pin_dc);
  start_spi_if_selected(c);
}

static void on_dc_change(void *user_data, pin_t pin, uint32_t value)
{
  chip_state_t *c = (chip_state_t *)user_data;
  stop_spi(c); // bytes so far belong to the previous D/C level
  c->dc_level = value;
  start_spi_if_selected(c);
}

static void on_rst_change(void *user_data, pin_t pin, uint32_t value)
{
  chip_state_t *c = (chip_state_t *)user_data;
  if (value == LOW)
  {
    stop_spi(c);
    for (uint32_t i = 0; i < NUM_PANELS; i++)
      reset_panel(&c->panels[i]);
    flush_all(c);
  }
  else
  {
    c->dc_level = pin_read(c->pin_dc);
    start_spi_if_selected(c);
  }
}

static void on_en_change(void *user_data, pin_t pin, uint32_t value)
{
  chip_state_t *c = (chip_state_t *)user_data;
  for (uint32_t i = 0; i < NUM_PANELS; i++)
    invalidate(&c->panels[i]);
  flush_all(c);
}

void chip_init(void)
{
  chip_state_t *c = &chip;
  static const char *cs_names[NUM_PANELS] = {"CS0", "CS1", "CS2", "CS3", "CS4", "CS5"};

  for (uint32_t i = 0; i < NUM_PANELS; i++)
    c->pin_cs[i] = pin_init(cs_names[i], INPUT_PULLUP);
  c->pin_dc = pin_init("DC", INPUT);
  c->pin_rst = pin_init("RES", INPUT_PULLUP);
  c->pin_en = pin_init("EN", INPUT);
  pin_init("VCC", INPUT);
  pin_init("GND", INPUT);

  c->col_offset = attr_read(attr_init("colOffset", 52));
  c->row_offset = attr_read(attr_init("rowOffset", 40));
  c->panel_inverted = attr_read(attr_init("panelInverted", 1)) != 0;
  if (c->col_offset + PANEL_W > GRAM_W)
    c->col_offset = GRAM_W - PANEL_W;
  if (c->row_offset + PANEL_H > GRAM_H)
    c->row_offset = GRAM_H - PANEL_H;

  c->framebuffer = framebuffer_init(&c->fb_w, &c->fb_h);
  // Panels are laid out left to right (CS0 = leftmost), the rest of the width is the gap
  c->gap = (c->fb_w > NUM_PANELS * PANEL_W) ? (c->fb_w - NUM_PANELS * PANEL_W) / (NUM_PANELS - 1) : 0;
  // Paint the whole display area black (the gaps between the panels are never written again)
  for (uint32_t i = 0; i < PANEL_W; i++)
    c->row_rgba[i] = 0xFF000000;
  for (uint32_t row = 0; row < c->fb_h; row++)
    for (uint32_t x = 0; x < c->fb_w; x += PANEL_W)
    {
      uint32_t n = (c->fb_w - x < PANEL_W) ? c->fb_w - x : PANEL_W;
      buffer_write(c->framebuffer, (row * c->fb_w + x) * 4, c->row_rgba, n * 4);
    }

  const spi_config_t spi_cfg = {
      .user_data = c,
      .sck = pin_init("SCL", INPUT),
      .mosi = pin_init("SDA", INPUT),
      .miso = NO_PIN,
      .mode = 0,
      .done = spi_done,
  };
  c->spi = spi_init(&spi_cfg);

  const pin_watch_config_t cs_watch = {.user_data = c, .edge = BOTH, .pin_change = on_cs_change};
  const pin_watch_config_t dc_watch = {.user_data = c, .edge = BOTH, .pin_change = on_dc_change};
  const pin_watch_config_t rst_watch = {.user_data = c, .edge = BOTH, .pin_change = on_rst_change};
  const pin_watch_config_t en_watch = {.user_data = c, .edge = BOTH, .pin_change = on_en_change};
  for (uint32_t i = 0; i < NUM_PANELS; i++)
    pin_watch(c->pin_cs[i], &cs_watch);
  pin_watch(c->pin_dc, &dc_watch);
  pin_watch(c->pin_rst, &rst_watch);
  pin_watch(c->pin_en, &en_watch);

  for (uint32_t i = 0; i < NUM_PANELS; i++)
    reset_panel(&c->panels[i]);
  flush_all(c);
  c->selected = read_selection(c);
  c->dc_level = pin_read(c->pin_dc);
  start_spi_if_selected(c);
}
