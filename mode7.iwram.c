#include <tonc.h>

#include "mode7.h"

static const int worldMap[24][24]= {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,2,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,3,0,0,0,3,0,0,0,1},
  {1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,2,2,0,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,5,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

IWRAM_CODE void m7_hbl() {
	int vc = REG_VCOUNT;
	int horz = m7_level.horizon;

	if (!IN_RANGE(vc, horz, SCREEN_HEIGHT)) {
		return;
	}

	if (vc == horz) {
		BFN_SET(REG_DISPCNT, DCNT_MODE1, DCNT_MODE);
		REG_BG2CNT = m7_level.bgcnt_floor;
	}
	
	BG_AFFINE *bga = &m7_level.bgaff[vc + 1];
	REG_BG_AFFINE[2] = *bga;

	REG_WIN0H = m7_level.winh[vc + 1];
	REG_WIN0V = 0 << 8 | SCREEN_HEIGHT;
}

IWRAM_CODE void m7_prep_affines(m7_level_t *level) {
	if (level->horizon >= SCREEN_HEIGHT) {
		return;
	}

	int start_line = (level->horizon >= 0) ? level->horizon : 0;

	m7_cam_t *cam = level->camera;

	/* location vector */
	FIXED a_x = cam->pos.x; // 8f
	FIXED a_y = cam->pos.y; // 8f
	FIXED a_z = cam->pos.z; // 8f
	a_x = int2fx(22);
	a_z = int2fx(12);

	/* sines and cosines of yaw, pitch */
	FIXED cos_phi   = cam->u.x; // 8f
	FIXED sin_phi   = cam->u.z; // 8f
	FIXED cos_theta = cam->v.y; // 8f
	FIXED sin_theta = cam->w.y; // 8f
	cos_theta = int2fx(-1);
	sin_theta = 0;

	BG_AFFINE *bg_aff_ptr = &level->bgaff[start_line];
	u16 *winh_ptr = &level->winh[start_line];

	FIXED plane_x = float2fx(0.0);
	FIXED plane_z = float2fx(0.66);
	for (int h = start_line; h < SCREEN_HEIGHT; h++) {
		/* ray intersect in camera plane */
		FIXED x_c = 2 * fxsub(fxdiv(int2fx(h), int2fx(SCREEN_HEIGHT)), int2fx(1));

		/* ray components in world space */
		FIXED ray_x = fxadd(cos_theta, fxmul(plane_x, x_c));
		if (ray_x == 0) {
			ray_x = 1;
		}
		FIXED ray_z = fxadd(sin_theta, fxmul(plane_z, x_c));
		if (ray_z == 0) {
			ray_z = 1;
		}

		/* map coordinates */
		int map_x = fx2int(a_x);
		int map_z = fx2int(a_z);

		/* ray lengths to next x / z side */
		FIXED delta_dist_x = fxdiv(int2fx(1), ray_x);
		FIXED delta_dist_z = fxdiv(int2fx(1), ray_z);

		/* initialize map / distance steps */
		int delta_map_x, delta_map_z;
		FIXED dist_x, dist_z;
		if (ray_x < 0) {
			delta_map_x = -1;
			dist_x = fxmul(fxsub(a_x, int2fx(map_x)), delta_dist_x);
		} else {
			delta_map_x = 1;
			dist_x = fxmul(fxadd(int2fx(map_x + 1), a_x), delta_dist_x);
		}
		if (ray_z < 0) {
			delta_map_z = -1;
			dist_z = fxmul(fxsub(a_z, int2fx(map_z)), delta_dist_z);
		} else {
			delta_map_z = 1;
			dist_z = fxmul(fxadd(int2fx(map_z + 1), a_z), delta_dist_z);
		}

		/* perform raycast */
		int hit = 0;
		int side;
		while (!hit) {
			if (dist_x < dist_z) {
				dist_x += delta_dist_x;
				map_x += delta_map_x;
				side = 0;
			} else {
				dist_z += delta_dist_z;
				map_z += delta_map_z;
				side = 1;
			}

			if (worldMap[map_x][map_z] > 0) {
				hit = 1;
			}
		}

		/* calculate wall distance */
		FIXED perp_wall_dist;
		if (side == 0) {
			perp_wall_dist = fxadd(fxsub(int2fx(map_x), a_x), fxdiv(int2fx(1 - delta_map_x), int2fx(2)));
		} else {
			perp_wall_dist = fxadd(fxsub(int2fx(map_z), a_z), fxdiv(int2fx(1 - delta_map_z), int2fx(2)));
		}
		if (perp_wall_dist == 0) {
			perp_wall_dist = 1;
		}

		int line_height = fx2int(fxdiv(int2fx(h), perp_wall_dist));
		int draw_start = -line_height / 2 + SCREEN_WIDTH / 2;
		if (draw_start < 0) { draw_start = 0; }
		int draw_end = line_height / 2 + SCREEN_WIDTH / 2;
		if (draw_end >= SCREEN_WIDTH) { draw_end = SCREEN_WIDTH; }

		/* apply windowing */
		*winh_ptr = draw_start << 8 | draw_end;
		winh_ptr++;

		/* build affine matrices */
		bg_aff_ptr->pa = perp_wall_dist;
		bg_aff_ptr->pd = perp_wall_dist;

		bg_aff_ptr->dx = 0;
		bg_aff_ptr->dy = int2fx(h);

		bg_aff_ptr++;
	}
	level->bgaff[SCREEN_HEIGHT] = level->bgaff[0];
	level->winh[SCREEN_HEIGHT] = level->winh[0];
}
