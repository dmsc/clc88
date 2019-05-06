#include <stdio.h>
#include "emu.h"
#include "cpu.h"
#include "cpuexec.h"
#include "screen.h"
#include "chroni.h"
#include "trace.h"


#define LOGTAG "CHRONI"

#define CPU_RUN(X) for(int nx=0; nx<X; nx++) CPU_GO(1)
#define CPU_SCANLINE() CPU_RUN(144-8);CPU_RESUME();CPU_RUN(8)
#define CPU_XPOS() if ((xpos++ & 3) == 0) CPU_GO(1)

#define VRAM_WORD(addr) (WORD(vram[addr], vram[addr+1]))
#define VRAM_PTR(addr) (VRAM_WORD(addr) << 1)


#define VRAM_MAX 128*1024

UINT8 vram[VRAM_MAX];

#define PAGE_OFFSET 0x0400

static UINT16 scanline;
static UINT8  page;

static UINT16 dl;
static UINT16 lms = 0;
static UINT16 attribs = 0;
static UINT16 ypos, xpos;

static UINT8 colors[4];
static UINT16 palette;

static UINT16 charset;
static UINT16 sprites;

// this is an RGB565 -> RGB888 conversion array for emulation only
static UINT8 rgb565[0x10000 * 3];
static UINT8 pixel_color_r;
static UINT8 pixel_color_g;
static UINT8 pixel_color_b;

#define STATUS_VBLANK 1
#define STATUS_HBLANK 2
#define STATUS_INTEN  4

static UINT8 status;
static UINT8 post_dli = 0;

void chroni_vram_write(UINT16 index, UINT8 value) {
	LOGV(LOGTAG, "vram write %04X = %02X", index, value);
	vram[page * PAGE_OFFSET + index] = value;
}

UINT8 chroni_vram_read(UINT16 index) {
	return vram[page * PAGE_OFFSET + index];
}

static void reg_low(UINT16 *reg, UINT8 value) {
	*reg = (*reg & 0xFF00) | value;
}

static void reg_high(UINT16 *reg, UINT8 value) {
	*reg = (*reg & 0x00FF) | (value << 8);
}

void chroni_register_write(UINT8 index, UINT8 value) {
	LOGV(LOGTAG, "chroni reg write: %04X = %02X", index, value);
	switch (index) {
	case 0:
		reg_low(&dl, value);
		break;
	case 1:
		reg_high(&dl, value);
		break;
	case 2:
		reg_low(&charset, value);
		break;
	case 3:
		reg_high(&charset, value);
		break;
	case 4:
		reg_low(&palette, value);
		break;
	case 5:
		reg_high(&palette, value);
		break;
	case 6:
		page = value & 0x7F;  // page offset in KB
		break;
	case 8:
		CPU_HALT();
		break;
	case 9:
		status = (status & 0x3) | (value & 0xFC);
		break;
	case 0xa:
		reg_low(&sprites, value);
		break;
	case 0xb:
		reg_high(&sprites, value);
		break;
	case 16:
	case 17:
	case 18:
	case 19:
		colors[index - 16] = value;
		break;
	}
}

UINT8 chroni_register_read(UINT8 index) {
	switch(index) {
	case 7: return ypos >> 1;
	case 9: return status;
	}
	return 0;
}

static inline void set_pixel_color(UINT8 color) {
	UINT16 pixel_color_rgb565 = vram[palette + color*2 + 0] + (vram[palette + color*2 + 1] << 8);

	pixel_color_r = rgb565[pixel_color_rgb565*3 + 0];
	pixel_color_g = rgb565[pixel_color_rgb565*3 + 1];
	pixel_color_b = rgb565[pixel_color_rgb565*3 + 2];
}

#define SPRITE_ATTR_ENABLED 0x10
#define SPRITE_SCAN_INVALID 0xFF

#define SPRITES_MAX   32
#define SPRITES_X     (SPRITES_MAX * 2)
#define SPRITES_Y     (SPRITES_MAX * 2 + SPRITES_X)
#define SPRITES_ATTR  (SPRITES_MAX * 2 + SPRITES_Y)
#define SPRITES_COLOR (SPRITES_MAX * 2 + SPRITES_ATTR)

static UINT8 sprite_scanlines[SPRITES_MAX];

static void do_scan_start() {
	status |= STATUS_HBLANK;
	if (post_dli && (status & STATUS_INTEN)) {
		LOGV(LOGTAG, "do_scan_start fire DLI");
		cpuexec_nmi(1);
	}
	post_dli = 0;

	CPU_RUN(22);
	status &= (255 - STATUS_HBLANK);
	cpuexec_nmi(0);

	/*
	 * check all sprites and write the scan to be drawn
	 * assume that the sprite will not be drawn
	 */
	for(int s=0; s < SPRITES_MAX; s++) {
		sprite_scanlines[s] = SPRITE_SCAN_INVALID; // asume invalid sprite for this scan

		UINT16 sprite_attrib = vram[sprites + SPRITES_ATTR + s*2];
		if ((sprite_attrib & SPRITE_ATTR_ENABLED) == 0) continue;

		int sprite_y = VRAM_WORD(sprites + SPRITES_Y + s*2) - 16;

		int sprite_scanline = scanline - sprite_y;
		if (sprite_scanline< 0 || sprite_scanline >=16) continue;
		sprite_scanlines[s] = sprite_scanline;
	}
}

static void do_scan_end() {
	CPU_RESUME();
	CPU_RUN(8);
}

static inline PAIR do_sprites() {
	UINT8 dot_color   = 0;
	UINT8 sprite_data = 0;
	for(int s=SPRITES_MAX-1; s>=0; s--) {
		UINT8 sprite_scanline = sprite_scanlines[s];
		if (sprite_scanline == SPRITE_SCAN_INVALID) continue;

		int sprite_x = VRAM_WORD(sprites + SPRITES_X + s*2) - 24;

		int sprite_pixel_x = xpos - sprite_x;
		if (sprite_pixel_x < 0) continue; // not yet
		if (sprite_pixel_x >=16) { // not anymore
			sprite_scanlines[s] = SPRITE_SCAN_INVALID;
			continue;
		}

		int sprite_pointer = VRAM_PTR(sprites + s*2);

		sprite_data = vram[sprite_pointer
				+ (sprite_scanline << 3)
				+ (sprite_pixel_x  >> 1)];
		sprite_data = (sprite_pixel_x & 1) == 0 ?
				sprite_data >> 4 :
				sprite_data & 0xF;
		if (sprite_data == 0) continue;

		UINT16 sprite_attrib = vram[sprites + SPRITES_ATTR + s*2];
		int sprite_palette = sprite_attrib & 0x0F;

		dot_color = vram[sprites + SPRITES_COLOR + sprite_palette*16 + sprite_data];
		break;
	}
	PAIR result;
	result.b.l = dot_color;
	result.b.h = sprite_data;
	return result;
}

static void inline put_pixel(int offset, UINT8 color) {
	PAIR sprite = do_sprites();
	UINT8 dot_color = sprite.b.h == 0 ? color : sprite.b.l;

	set_pixel_color(dot_color);
	screen[offset + xpos*3 + 0] = pixel_color_r;
	screen[offset + xpos*3 + 1] = pixel_color_g;
	screen[offset + xpos*3 + 2] = pixel_color_b;
	CPU_XPOS();
}

static void inline do_border(int offset, int size) {
	for(int i=0; i<size; i++) {
		put_pixel(offset, colors[0]);
	}
}

static void do_scan_blank() {
	do_scan_start();

	int offset = scanline * screen_pitch;
	xpos = 0;
	do_border(offset, screen_width);
	do_scan_end();
}

static void do_scan_text(UINT8 line) {
	LOGV(LOGTAG, "do_scan_text line %d", line);
	do_scan_start();

	int offset = scanline * screen_pitch;
	xpos = 0;
	do_border(offset, SCREEN_XBORDER);

	UINT8 row;
	int char_offset = 0;
	for(int i=0; i<SCREEN_XRES; i++) {
		if (i % 8 == 0) {
			UINT8 c = vram[lms + char_offset];
			row = vram[charset + c*8 + line];
			char_offset++;
		}

		put_pixel(offset, colors[row & 0x80 ? 2 : 1]);
		row *= 2;
	}

	do_border(offset, SCREEN_XBORDER);
	do_scan_end();
}

static void do_scan_text_attribs(UINT8 line) {
	LOGV(LOGTAG, "do_scan_text_attribs line %d", line);
	do_scan_start();

	int offset = scanline * screen_pitch;
	xpos = 0;
	do_border(offset, SCREEN_XBORDER);

	UINT8 row;
	UINT8 foreground, background;
	int char_offset = 0;
	for(int i=0; i<SCREEN_XRES; i++) {
		if (i % 8 == 0) {
			UINT8 attrib = vram[attribs + char_offset];
			foreground = (attrib & 0xF0) >> 4;
			background = attrib & 0x0F;

			UINT8 c = vram[lms + char_offset];
			row = vram[charset + c*8 + line];
			char_offset++;
		}

		put_pixel(offset, row & 0x80 ? foreground : background);
		row *= 2;
	}

	do_border(offset, SCREEN_XBORDER);
	do_scan_end();
}

static void do_screen() {
	/* 0-7 scanlines are not displayed because of vblank
	 *
	 */
	for(ypos = 0; ypos <8; ypos++) {
		CPU_SCANLINE();
	}
	cpuexec_nmi(0);
	status &= (255 - STATUS_VBLANK);
	LOGV(LOGTAG, "set status %02X", status);

	scanline = 0;

	UINT8 instruction;
	int dlpos = 0;
	while(ypos < screen_height) {
		instruction = vram[dl + dlpos];
		int scan_post_dli = instruction & 0x80;
		LOGV(LOGTAG, "DL instruction %04X = %02X", dl + dlpos, instruction);
		dlpos++;
		if ((instruction & 7) == 0) { // blank lines
			UINT8 lines = 1 + ((instruction & 0x70) >> 4);
			LOGV(LOGTAG, "do_scan_blank lines %d", lines);
			for(int line=0; line<lines; line++) {
				if (line == lines - 1) post_dli = scan_post_dli;
				do_scan_blank();
				scanline++;
				ypos++;
				if (ypos == screen_height) return;
			}
		} else if ((instruction & 7) == 2) {
			if (instruction & 64) {
				lms = vram[dl + dlpos++];
				lms += 256*vram[dl + dlpos++];
			}
			LOGV(LOGTAG, "do_scan_text lms: %04X", lms);
			int lines = 8;
			for(int line=0; line<lines; line++) {
				if (line == lines - 1) post_dli = scan_post_dli;
				do_scan_text(line);
				scanline++;
				ypos++;
				if (ypos == screen_height) return;
			}

			lms += 40;
		} else if ((instruction & 7) == 3) {
			if (instruction & 64) {
				lms = vram[dl + dlpos++];
				lms += 256*vram[dl + dlpos++];
				attribs = vram[dl + dlpos++];
				attribs += 256*vram[dl + dlpos++];
			}
			LOGV(LOGTAG, "do_scan_text_attrib lms: %04X attrib: %04X", lms, attribs);
			int lines = 8;
			for(int line=0; line<8; line++) {
				if (line == lines - 1) post_dli = scan_post_dli;
				do_scan_text_attribs(line);
				scanline++;
				ypos++;
				if (ypos == screen_height) return;
			}

			lms += 40;
			attribs += 40;
		} else if (instruction == 0x41) {
			break;
		}
	}
	for(;scanline <screen_height; scanline++) {
		do_scan_blank();
		ypos++;
	}
}

static void init_rgb565_table() {
	for(int c=0; c<0x10000; c++) {
		UINT8 r = ((c & 0xF800) >> 11) * (256 / 32);
		UINT8 g = ((c & 0X07E0) >> 5)  * (256 / 64);
		UINT8 b = (c & 0X001F) * (256 / 32);

		rgb565[c*3 + 0] = b;
		rgb565[c*3 + 1] = g;
		rgb565[c*3 + 2] = r;
	}
}

void chroni_init() {
	init_rgb565_table();
}

void chroni_run_frame() {
	do_screen();
	status |= STATUS_VBLANK;
	if (status & STATUS_INTEN) cpuexec_nmi(1);
}
