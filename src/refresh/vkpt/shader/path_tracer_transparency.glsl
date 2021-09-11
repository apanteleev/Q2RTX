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

float erf(float x)
{
    // Approximation of the error function, erf.
    // See https://en.wikipedia.org/wiki/Error_function#Numerical_approximations 

    float ax = abs(x);
    float ax2 = ax * ax;
    float ax3 = ax2 * ax;
    float ax4 = ax2 * ax2;

    vec4 c = vec4(0.278393f, 0.230389f, 0.000972f, 0.078108f);
    vec4 powers = vec4(ax, ax2, ax3, ax4);

    float t = 1.f + dot(c, powers);
    float t2 = t * t;
    float t4 = t2 * t2;

    float v = 1.f - 1.f / t4;
    if (x < 0) v = -v;

    return v;
}

float extinction_integral(float mean, float sigma, float invSigma, float densityScale, float t1, float t2)
{
    // Compute light attenuation in a medium with non-uniform density.
    // The density is a normal distribution with parameters (mean, sigma) along the ray starting at t = 0.

    const float invSqrt2 = 0.7071067812f;
    const float sqrtHalfPi = 1.2533141373f;

    float erf_t1 = erf((t1 - mean) * invSqrt2 * invSigma);
    float erf_t2 = erf((t2 - mean) * invSqrt2 * invSigma);

    return exp(-sqrtHalfPi * densityScale * sigma * (erf_t2 - erf_t1));
}

vec4 get_fog_color(vec3 origin, vec3 dir, float t1, float t2)
{
	t2 = min(t2, global_ubo.fog_range);
	if (t2 < t1) return vec4(0);

	float inv_dir_z = 1.0 / dir.z;
	float sigma_t = global_ubo.fog_thickness * inv_dir_z;
	float mean_t = (global_ubo.fog_mean_height - origin.z) * inv_dir_z;
	float inv_sigma_t = dir.z * global_ubo.fog_inv_thickness;
	float alpha = 1.0 - extinction_integral(mean_t, sigma_t, inv_sigma_t, global_ubo.fog_density, t1, t2);

	float intensity = min(global_ubo.prev_adapted_luminance * 10, 0.02);
	return vec4(global_ubo.fog_color * alpha * intensity, alpha);
}

void update_payload_transparency(inout RayPayload rp, vec3 origin, vec3 direction, vec4 color, float thickness, float hitT)
{
	if(hitT > rp.farthest_transparent_distance)
	{
		vec4 farthest_transparency = unpackHalf4x16(rp.farthest_transparency);
		if (rp.closest_max_transparent_distance > 0)
		{
			vec4 fog = get_fog_color(origin, direction, rp.closest_max_transparent_distance, rp.farthest_transparent_distance);
			farthest_transparency = alpha_blend_premultiplied(fog, farthest_transparency);
		}

		rp.close_transparencies = packHalf4x16(alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), farthest_transparency));
		rp.closest_max_transparent_distance = rp.farthest_transparent_distance;
		rp.farthest_transparency = packHalf4x16(color);
		rp.farthest_transparent_distance = hitT;
		rp.farthest_transparent_depth = thickness;
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

vec4 get_payload_transparency(in RayPayload rp, vec3 origin, vec3 direction, float solidDist)
{
	vec4 accumulator = vec4(0);
	float current_dist = solidDist;

	if (rp.farthest_transparent_distance > 0)
	{
		accumulator = get_fog_color(origin, direction, rp.farthest_transparent_distance, solidDist);

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
		vec4 fog = get_fog_color(origin, direction, rp.closest_max_transparent_distance, current_dist);
		accumulator = alpha_blend_premultiplied(fog, accumulator);
		accumulator = alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), accumulator);
		current_dist = rp.closest_max_transparent_distance;
	}

	vec4 fog = get_fog_color(origin, direction, 0, current_dist);
	accumulator = alpha_blend_premultiplied(fog, accumulator);

	return accumulator;
}

vec4 get_payload_transparency_simple(in RayPayload rp)
{
	return alpha_blend_premultiplied(unpackHalf4x16(rp.close_transparencies), unpackHalf4x16(rp.farthest_transparency));
}

#endif // PATH_TRACER_TRANSPARENCY_GLSL_
