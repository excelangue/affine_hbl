#include <limits.h>
#include <stdio.h>

#include <tonc.h>

#include "mode7.h"

#include "gfx/fanroom.h"
#include "gfx/bgpal.h"

#define DEBUG(fmt, ...)
#define DEBUGFMT(fmt, ...)

/*
#include <stdlib.h>
char *dbg_str;
#define DEBUG(str) (nocash_puts(str))
#define DEBUGFMT(fmt, ...) do {	  \
		asprintf(&dbg_str, fmt, __VA_ARGS__); \
		nocash_puts(dbg_str); \
		free(dbg_str); \
	} while (0)
*/

/* block mappings */
#define M7_CBB 0
#define FLOOR_SBB 24
#define TTE_CBB 1
#define TTE_SBB 20
#define M7_PRIO 3

/* m7 globals */
m7_cam_t m7_cam;
BG_AFFINE m7_aff_arrs[SCREEN_HEIGHT+1];
u16 m7_winh[SCREEN_HEIGHT + 1];
m7_level_t m7_level;

static const m7_cam_t m7_cam_default = {
	{ 0x0, 2 << FIX_SHIFT, 2 << FIX_SHIFT }, /* pos */
	0x0000, /* theta */
	0x0, /* phi */
	{1 << FIX_SHIFT, 0, 0}, /* u */
	{0, 1 << FIX_SHIFT, 0}, /* v */
	{0, 0, 1 << FIX_SHIFT}  /* w */
};

/* prototypes */

void init_main();

void input_game();
void camera_update();

/* implementations */

const int blocks[16 * 32] = {
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,2, 3,3,3,3,3,3,3,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,2,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,2,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,2,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,2,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,2,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,2,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1
};

void init_map() {
	/* layout level */
	m7_level.blocks = (int*)blocks;
	m7_level.blocks_width = 32;
	m7_level.blocks_height = 16;
	m7_level.texture_width = 512;
	m7_level.pixels_per_block = int2fx(m7_level.texture_width / m7_level.blocks_width);

	/* init mode 7 */
	m7_init(&m7_level, &m7_cam, m7_aff_arrs, m7_winh,
		BG_CBB(M7_CBB) | BG_SBB(FLOOR_SBB) | BG_AFF_64x64 | BG_WRAP | BG_PRIO(M7_PRIO));
	*(m7_level.camera) = m7_cam_default;

	m7_level.camera->fov = fxdiv(int2fx(M7_TOP), int2fx(M7_D));

	/* extract main bg */
	LZ77UnCompVram(bgPal, pal_bg_mem);
	LZ77UnCompVram(fanroomTiles, tile_mem[M7_CBB]);
	LZ77UnCompVram(fanroomMap, se_mem[FLOOR_SBB]);

	/* setup orange fade */
	REG_BLDCNT = BLD_BUILD(BLD_BG2, BLD_BACKDROP, 1);
	pal_bg_mem[0] = CLR_ORANGE;

	/* registers */
	REG_DISPCNT = DCNT_MODE1 | DCNT_BG0 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_WIN0;
	REG_WININ = WININ_BUILD(WIN_BG2, 0);
	REG_WINOUT = WINOUT_BUILD(WIN_BG0, 0);
}

const FIXED OMEGA =  0x400;
const FIXED VEL_X =  0x1 << 8;
const FIXED VEL_Z = -0x1 << 8;
void input_game(VECTOR *dir) {
	key_poll();

	/* strafe */
	dir->x = VEL_X * key_tri_shoulder();

	/* forwards / backwards */
	dir->z = VEL_Z * key_tri_vert();

	/* rotate */
	m7_level.camera->theta -= OMEGA * key_tri_horz();
}

void camera_update(VECTOR *dir) {
	/* update camera rotation */
	m7_rotate(m7_level.camera, m7_level.camera->theta);

	/* update camera position */
	m7_translate_local(&m7_level, dir);

	/* don't sink through the ground */
	if(m7_level.camera->pos.y < (2 << 8)) {
		m7_level.camera->pos.y = 2 << 8;
	}
}

int main() {
	init_map();

	/* hud */
	tte_init_chr4c_b4_default(0, BG_CBB(TTE_CBB) | BG_SBB(TTE_SBB));
	tte_init_con();
	tte_set_margins(8, 8, 232, 40);

	/* irqs */
	irq_init(NULL);
	irq_add(II_HBLANK, (fnptr)m7_hbl);
	irq_add(II_VBLANK, NULL);

	while(1) {
		VBlankIntrWait();

		/* update camera based on input */
		VECTOR dir = {0, 0, 0};
		input_game(&dir);
		camera_update(&dir);

		/* update affine matrices */
		m7_prep_affines(&m7_level);

		/* update hud */
		tte_printf("#{es;P}x %x fov %x",
			m7_level.camera->pos.x, m7_level.camera->fov);
	}

	return 0;

}
