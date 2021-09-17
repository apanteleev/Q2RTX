/*
Copyright (C) 2021, NVIDIA CORPORATION. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __FOG_H_
#define __FOG_H_

#include <shared/shared.h>

#define MAX_FOG_VOLUMES 8

typedef struct
{
	vec3_t mins;
	vec3_t maxs;
	vec3_t color;
	float density;
	int softface; // 0 = none, 1 = x1, 2 = x2, 3 = y1, 4 = y2, 5 = z1, 6 = z2
} fog_volume_t;

extern fog_volume_t fog_volumes[MAX_FOG_VOLUMES];

void vkpt_fog_init(void);
void vkpt_fog_shutdown(void);
void vkpt_fog_reset(void);

#endif // __FOG_H_
