#include <limits.h>
#include <tonc.h>

#include "mode7.h"

M7Camera::M7Camera(FIXED f) {
	pos = { 8 << FIX_SHIFT, 2 << FIX_SHIFT, 2 << FIX_SHIFT };
	theta = 0x0000;
	phi = 0x0;

	u = {1 << FIX_SHIFT, 0, 0};
	v = {0, 1 << FIX_SHIFT, 0};
	w = {0, 0, 1 << FIX_SHIFT};
	fov = f;
}

void M7Camera::rotate(FIXED th) {
	/* limited to fixpoint range */
	th &= 0xFFFF;

	FIXED ct = lu_cos(th) >> 4;
	FIXED st = lu_sin(th) >> 4;

	/* camera x-axis */
	u = {1 << 8, 0, 0};

	/* camera y-axis */
	v = {0, ct, -1 * st};

	/* camera z-axis */
	w = {0, st, ct};
}

M7Map::M7Map(u16 bgc, const int *bl, int bw, int bh, int bd, FIXED *ew, FIXED *eo,
	FIXED fov, const FIXED *extents) :
	bgcnt(bgc), blocks(bl), blocksWidth(bw), blocksHeight(bh), blocksDepth(bd),
	extentWidths(ew), extentOffs(eo) {

	/* calculate texture dimensions */
	textureWidth = blocksWidth * PIX_PER_BLOCK;
	textureHeight = blocksHeight * PIX_PER_BLOCK;
	textureDepth = blocksDepth * PIX_PER_BLOCK;

	/* precalculate extent widths */
	for (int i = 0; i < blocksHeight; i++) {
		extentWidths[i] = fxmul(
			int2fx(
				(extents[i * 2 + 1] - extents[i * 2])
				* PIX_PER_BLOCK), // normal extent width
			fov); // adjust for fov
		extentOffs[i] = (int2fx(blocksWidth + extents[i * 2])) / 2;
	}
}

M7Level::M7Level(M7Camera *c, M7Map *m1, M7Map *m2) : cam(c) {
	maps[0] = m1;
	maps[1] = m2;

	/* apply background control regs */
	REG_BG3CNT = m1->bgcnt;
	REG_BG2CNT = m2->bgcnt;
}

void M7Level::translateLocal(const VECTOR *dir) {
	M7Map *m = maps[0];

	VECTOR p = cam->pos;
	p.x += (cam->u.x * dir->x + cam->v.x * dir->y + cam->w.x * dir->z) >> 8;
	p.y += ( 0                + cam->v.y * dir->y + cam->w.y * dir->z) >> 8;
	p.z += (cam->u.z * dir->x + cam->v.z * dir->y + cam->w.z * dir->z) >> 8;

	const int mapX = fx2int(p.x);
	const int mapY = fx2int(p.y);
	const int mapZ = fx2int(p.z);

	/* update x */
	if ((0 <= mapX) && (mapX <= m->blocksWidth)) {
		cam->pos.x = p.x;
	}

	/* check y / z wall collision independently */
	if ((mapY >= 0) && (mapY < m->blocksHeight)) {
		if (m->blocks[(mapY * m->blocksDepth) + fx2int(cam->pos.z)] == 0) {
			cam->pos.y = p.y;
		}
	}
	if ((mapX >= 0) && (mapZ < m->blocksDepth)) {
		if (m->blocks[(fx2int(cam->pos.y) * m->blocksDepth) + mapZ] == 0) {
			cam->pos.z = p.z;
		}
	}
}