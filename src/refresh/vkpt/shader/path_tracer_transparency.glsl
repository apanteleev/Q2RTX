/*
Copyright (C) 2018 Christoph Schied
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

#ifndef PATH_TRACER_TRANSPARENCY_GLSL_
#define PATH_TRACER_TRANSPARENCY_GLSL_

vec4 get_fog_color(float distance)
{
	float alpha = 1.0 - exp(-global_ubo.fog_density * abs(distance));
	float intensity = min(global_ubo.prev_adapted_luminance * 10, 0.02);
	return vec4(global_ubo.fog_color * alpha * intensity, alpha);
}

void update_payload_transparency(inout RayPayload rp, vec4 color, float depth, float hitT)
{
	if (color.a <= 0)
		return;

	if(hitT > rp.farthest_transparent_distance)
	{
		vec4 farthest_transparency = unpackHalf4x16(rp.farthest_transparency);
		if (rp.closest_max_transparent_distance > 0)
		{
			vec4 fog = get_fog_color(rp.farthest_transparent_distance - rp.closest_max_transparent_distance);
			farthest_transparency = alpha_blend_premultiplied(fog, farthest_transparency);
		}

		rp.close_transparencies = packHalf4x16(alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), farthest_transparency));
		rp.closest_max_transparent_distance = rp.farthest_transparent_distance;
		rp.farthest_transparency = packHalf4x16(color);
		rp.farthest_transparent_distance = hitT;
		rp.farthest_transparent_depth = depth;
	}
	else if(rp.closest_max_transparent_distance < hitT)
	{
		rp.close_transparencies = packHalf4x16(alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), color));
		rp.closest_max_transparent_distance = hitT;
	}
	else
	{
		rp.close_transparencies = packHalf4x16(alpha_blend_premultiplied(color, unpackHalf4x16(rp.close_transparencies)));
	}
}

vec4 get_payload_transparency(in RayPayload rp, float solidDist)
{
	vec4 accumulator = vec4(0);
	float current_dist = solidDist;

	if (rp.farthest_transparent_distance > 0)
	{
		accumulator = get_fog_color(solidDist - rp.farthest_transparent_distance);

		vec4 farthest_transparency = unpackHalf4x16(rp.farthest_transparency);

		if (rp.farthest_transparent_depth > 0)
		{
			farthest_transparency *= clamp((solidDist - rp.farthest_transparent_distance) / rp.farthest_transparent_depth, 0, 1);
		}

		accumulator = alpha_blend_premultiplied(farthest_transparency, accumulator);
		current_dist = rp.farthest_transparent_distance;
	}

	if (rp.closest_max_transparent_distance > 0)
	{
		vec4 fog = get_fog_color(current_dist - rp.closest_max_transparent_distance);
		accumulator = alpha_blend_premultiplied(fog, accumulator);
		accumulator = alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), accumulator);
		current_dist = rp.closest_max_transparent_distance;
	}

	vec4 fog = get_fog_color(current_dist);
	accumulator = alpha_blend_premultiplied(fog, accumulator);

	return accumulator;
}

vec4 get_payload_transparency_simple(in RayPayload rp)
{
	return alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), unpackHalf4x16(rp.farthest_transparency));
}

#endif // PATH_TRACER_TRANSPARENCY_GLSL_
