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

#include "fog.h"
#include "refresh/refresh.h"
#include "shader/global_ubo.h"
#include <common/prompt.h>

#include <string.h>

fog_volume_t fog_volumes[MAX_FOG_VOLUMES];


static const cmd_option_t o_fog[] = {
	{ "v:int", "volume", "volume index" },
	{ "p", "print", "print the selected volume to the console" },
	{ "r", "reset", "reset the selected volume" },
	{ "R", "reset-all", "reset all volumes" },
	{ "a:x,y,z", "mins", "fog volume min bounds" },
	{ "b:x,y,z", "maxs", "fog volume max bounds" },
	{ "c:r,g,b", "color", "fog color" },
	{ "d:float", "density", "fog density" },
	{ "f:face", "softface", "face where the density is zero: none, xa, xb, ya, yb, za, zb" },
	{ "h", "help", "display this message" },
	{ NULL }
};

static const char* o_softface[] = {
	"none", "xa", "xb", "ya", "yb", "za", "zb"
};


static void Fog_Cmd_c(genctx_t* ctx, int argnum)
{
	Cmd_Option_c(o_fog, NULL, ctx, argnum);
}

static void Fog_Cmd_f(void)
{
	fog_volume_t* volume = NULL;
	float x, y, z;
	int index = -1;
	int c, i;
	while ((c = Cmd_ParseOptions(o_fog)) != -1) {
		switch(c)
		{
		case 'h':
			Cmd_PrintUsage(o_fog, NULL);
			Com_Printf("Set parameters of a fog volume.\n");
			Cmd_PrintHelp(o_fog);
			return;
		case 'v':
			if (1 != sscanf(cmd_optarg, "%d", &index) || index < 0 || index >= MAX_FOG_VOLUMES) {
				Com_WPrintf("invalid volume index '%s'\n", cmd_optarg);
				return;
			}
			volume = fog_volumes + index;
			break;
		case 'p':
			if (!volume) goto no_volume;
			Com_Printf("fog volume %d:\n", index);
			Com_Printf("    mins %f,%f,%f\n", volume->mins[0], volume->mins[1], volume->mins[2]);
			Com_Printf("    maxs %f,%f,%f\n", volume->maxs[0], volume->maxs[1], volume->maxs[2]);
			Com_Printf("    color %f,%f,%f\n", volume->color[0], volume->color[1], volume->color[2]);
			Com_Printf("    density %f\n", volume->density);
			Com_Printf("    softface %s\n", o_softface[volume->softface]);
			break;
		case 'r':
			if (!volume) goto no_volume;
			memset(volume, 0, sizeof(*volume));
			break;
		case 'R':
			memset(fog_volumes, 0, sizeof(fog_volumes));
			break;
		case 'a':
			if (!volume) goto no_volume;
			if (3 != sscanf(cmd_optarg, "%f,%f,%f", &x, &y, &z)) {
				Com_WPrintf("invalid coordinates '%s'\n", cmd_optarg);
				return;
			}
			volume->mins[0] = x;
			volume->mins[1] = y;
			volume->mins[2] = z;
			break;
		case 'b':
			if (!volume) goto no_volume;
			if (3 != sscanf(cmd_optarg, "%f,%f,%f", &x, &y, &z)) {
				Com_WPrintf("invalid coordinates '%s'\n", cmd_optarg);
				return;
			}
			volume->maxs[0] = x;
			volume->maxs[1] = y;
			volume->maxs[2] = z;
			break;
		case 'c':
			if (!volume) goto no_volume;
			if (3 != sscanf(cmd_optarg, "%f,%f,%f", &x, &y, &z)) {
				Com_WPrintf("invalid color '%s'\n", cmd_optarg);
				return;
			}
			volume->color[0] = x;
			volume->color[1] = y;
			volume->color[2] = z;
			break;
		case 'd':
			if (!volume) goto no_volume;
			if (1 != sscanf(cmd_optarg, "%f", &x)) {
				Com_WPrintf("invalid density '%s'\n", cmd_optarg);
				return;
			}
			volume->density = x;
			break;
		case 'f':
			if (!volume) goto no_volume;
			for (i = 0; i < (int)q_countof(o_softface); i++) {
				if (strcmp(cmd_optarg, o_softface[i]) == 0) {
					volume->softface = i;
					break;
				}
			}
			if (i >= (int)q_countof(o_softface)) {
				Com_WPrintf("invalid value for softface '%s'\n", cmd_optarg);
				return;
			}
			break;
		default:
			return;
		}
	}
	return;
	
no_volume:
	Com_WPrintf("volume not specified\n");
}

void vkpt_fog_init(void)
{
	vkpt_fog_reset();
	
	cmdreg_t cmds[] = {
		{ "fog", &Fog_Cmd_f, &Fog_Cmd_c },
		{ NULL, NULL, NULL }
	};
	Cmd_Register(cmds);
}

void vkpt_fog_shutdown(void)
{
	Cmd_RemoveCommand("fog");
}

void vkpt_fog_reset(void)
{
	memset(fog_volumes, 0, sizeof(fog_volumes));
}

void vkpt_fog_upload(ShaderFogVolume_t* dst)
{
	memset(dst, 0, sizeof(ShaderFogVolume_t) * MAX_FOG_VOLUMES);
	
	for (int i = 0; i < MAX_FOG_VOLUMES; i++)
	{
		const fog_volume_t* src = fog_volumes + i;
		if (src->density <= 0.f || src->mins[0] >= src->maxs[0] || src->mins[1] >= src->maxs[1] || src->mins[2] >= src->maxs[2])
			continue;

		VectorCopy(src->color, dst->color);
		VectorCopy(src->mins, dst->mins);
		VectorCopy(src->maxs, dst->maxs);

		if (1 <= src->softface && src->softface <= 6)
		{
			// Find the axis along which the density gradient is happening: x, y or z
			int axis = (src->softface - 1) / 2;

			// Find the positions on that axis where the density multiplier is 0 (pos0) and 1 (pos1)
			float pos0 = (src->softface & 1) ? src->mins[axis] : src->maxs[axis];
			float pos1 = (src->softface & 1) ? src->maxs[axis] : src->mins[axis];

			// Derive the linear function of the form (ax + b) that describes the density along the axis
			float a = src->density / (pos1 - pos0);
			float b = -pos0 * a;

			// Convert the 1D linear funciton into a volumetric one
			dst->density[axis] = a;
			dst->density[3] = b;
		}
		else
		{
			// No density gradient, just store the density with 0 spatial coefficinents
			Vector4Set(dst->density, 0.f, 0.f, 0.f, src->density);
		}
		
		dst->is_active = 1;

		++dst;
	}
}
