#include <limits.h>
#include <tonc.h>

#include "mode7.h"

INLINE int m7_horizon_line(const m7_level_t *level) {
	return clamp(level->horizon, 0, 228);
}

void m7_init(m7_level_t *level, m7_cam_t *cam, BG_AFFINE bgaff[], u16 winh_arr[], u16 skycnt, u16 floorcnt) {
	level->camera = cam;
	level->bgaff = bgaff;
	level->winh = winh_arr;
	level->bgcnt_sky = skycnt;
	level->bgcnt_floor = floorcnt;
	level->horizon = 80;

	REG_BG2CNT = floorcnt;
	REG_BG_AFFINE[2] = bg_aff_default;
}


void m7_prep_horizon(m7_level_t *level) {
	m7_cam_t *cam = level->camera;

	int horz = 0;
	if (cam->v.y != 0) {
		horz = (M7_FAR_BG * cam->w.y) - cam->pos.y;
		horz = M7_TOP - Div(horz * M7_D, M7_FAR_BG * cam->v.y);
		/* horz = M7_TOP - Div(cam->w.y*M7_D, cam->v.y); */
	} else {
		/* looking straight down */
		horz = (cam->w.y > 0) ? INT_MIN : INT_MAX;
	}
	
	level->horizon = 0;//horz;
}

void m7_rotate(m7_cam_t *cam, int phi, int theta) {
	cam->phi = phi;
	cam->theta = theta;

	FIXED cf, sf, ct, st;
	cf = lu_cos(phi) >> 4;
	sf = lu_sin(phi) >> 4;
	ct = lu_cos(theta) >> 4;
	st = lu_sin(theta) >> 4;

	/* camera x-axis */
	cam->u.x = cf;
	cam->u.y = 0;
	cam->u.z = sf;

	/* camera y-axis */
	cam->v.x = (sf * st) >> 8;
	cam->v.y = ct;
	cam->v.z = (-cf * st) >> 8;

	/* camera z-axis */
	cam->w.x = (-sf * ct) >> 8;
	cam->w.y = st;
	cam->w.z = (cf * ct) >> 8;
}

void m7_translate_local(m7_level_t *level, const VECTOR *dir) {
	m7_cam_t *cam = level->camera;

	VECTOR pos = cam->pos;
	pos.x += (cam->u.x * dir->x + cam->v.x * dir->y + cam->w.x * dir->z) >> 8;
	pos.y += ( 0                + cam->v.y * dir->y + cam->w.y * dir->z) >> 8;
	pos.z += (cam->u.z * dir->x + cam->v.z * dir->y + cam->w.z * dir->z) >> 8;

	/* update x */
	cam->pos.x = pos.x;

	/* check y / z wall collision independently */
	if ((pos.y >= 0) && (fx2int(pos.y) < level->blocks_height)) {
		if (level->blocks[(fx2int(pos.y) * m7_level.blocks_width) + fx2int(cam->pos.z)] == 0) {
			cam->pos.y = pos.y;
		}
	}
	if ((pos.z >= 0) && (fx2int(pos.z) < level->blocks_width)) {
		if (level->blocks[(fx2int(cam->pos.y) * m7_level.blocks_width) + fx2int(pos.z)] == 0) {
			cam->pos.z = pos.z;
		}
	}
}

void m7_translate_level(m7_level_t *level, const VECTOR *dir) {
	m7_cam_t *cam = level->camera;

	cam->pos.x += ((cam->u.x * dir->x) - (cam->u.z * dir->z)) >> 8;
	cam->pos.y += dir->y;
	cam->pos.z += ((cam->u.z * dir->x) + (cam->u.x * dir->z)) >> 8;
}