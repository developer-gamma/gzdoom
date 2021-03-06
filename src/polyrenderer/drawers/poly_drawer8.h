/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include "screen_triangle.h"

namespace TriScreenDrawerModes
{
	template<typename SamplerT>
	FORCEINLINE unsigned int Sample8(int32_t u, int32_t v, const uint8_t *texPixels, int texWidth, int texHeight, uint32_t color, const uint8_t *translation)
	{
		uint8_t texel;
		if (SamplerT::Mode == (int)Samplers::Shaded || SamplerT::Mode == (int)Samplers::Stencil || SamplerT::Mode == (int)Samplers::Fill || SamplerT::Mode == (int)Samplers::Fuzz || SamplerT::Mode == (int)Samplers::FogBoundary)
		{
			return color;
		}
		else if (SamplerT::Mode == (int)Samplers::Translated)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			return translation[texPixels[texelX * texHeight + texelY]];
		}
		else
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			texel = texPixels[texelX * texHeight + texelY];
		}

		if (SamplerT::Mode == (int)Samplers::Skycap)
		{
			int start_fade = 2; // How fast it should fade out

			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			if (a == 256)
				return texel;

			uint32_t capcolor = GPalette.BaseColors[color].d;
			uint32_t texelrgb = GPalette.BaseColors[texel].d;
			uint32_t r = RPART(texelrgb);
			uint32_t g = GPART(texelrgb);
			uint32_t b = BPART(texelrgb);
			uint32_t capcolor_red = RPART(capcolor);
			uint32_t capcolor_green = GPART(capcolor);
			uint32_t capcolor_blue = BPART(capcolor);
			r = (r * a + capcolor_red * inv_a + 127) >> 8;
			g = (g * a + capcolor_green * inv_a + 127) >> 8;
			b = (b * a + capcolor_blue * inv_a + 127) >> 8;
			return RGB256k.All[((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2)];
		}
		else
		{
			return texel;
		}
	}

	template<typename SamplerT>
	FORCEINLINE unsigned int SampleShade8(int32_t u, int32_t v, const uint8_t *texPixels, int texWidth, int texHeight, int x, int y)
	{
		if (SamplerT::Mode == (int)Samplers::Shaded)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = texPixels[texelX * texHeight + texelY];
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			return sampleshadeout;
		}
		else if (SamplerT::Mode == (int)Samplers::Stencil)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			return texPixels[texelX * texHeight + texelY] != 0 ? 256 : 0;
		}
		else if (SamplerT::Mode == (int)Samplers::Fuzz)
		{
			using namespace swrenderer;

			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = (texPixels[texelX * texHeight + texelY] != 0) ? 256 : 0;

			fixed_t fuzzscale = (200 << FRACBITS) / viewheight;

			int scaled_x = (x * fuzzscale) >> FRACBITS;
			int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + fuzzpos;

			fixed_t fuzzcount = FUZZTABLE << FRACBITS;
			fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
			unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

			sampleshadeout = (sampleshadeout * alpha) >> 5;
			return sampleshadeout;
		}
		else
		{
			return 0;
		}
	}

	template<typename BlendT>
	FORCEINLINE uint8_t ShadeAndBlend8(uint8_t fgcolor, uint8_t bgcolor, uint32_t fgshade, uint32_t lightshade, const uint8_t *colormaps, uint32_t srcalpha, uint32_t destalpha)
	{
		lightshade = ((256 - lightshade) * NUMCOLORMAPS) & 0xffffff00;
		uint8_t shadedfg = colormaps[lightshade + fgcolor];

		if (BlendT::Mode == (int)BlendModes::Opaque)
		{
			return shadedfg;
		}
		else if (BlendT::Mode == (int)BlendModes::Masked)
		{
			return (fgcolor != 0) ? shadedfg : bgcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddSrcColorOneMinusSrcColor)
		{
			int32_t fg_r = GPalette.BaseColors[shadedfg].r;
			int32_t fg_g = GPalette.BaseColors[shadedfg].g;
			int32_t fg_b = GPalette.BaseColors[shadedfg].b;
			int32_t bg_r = GPalette.BaseColors[bgcolor].r;
			int32_t bg_g = GPalette.BaseColors[bgcolor].g;
			int32_t bg_b = GPalette.BaseColors[bgcolor].b;
			int32_t inv_fg_r = 256 - (fg_r + (fg_r >> 7));
			int32_t inv_fg_g = 256 - (fg_g + (fg_g >> 7));
			int32_t inv_fg_b = 256 - (fg_b + (fg_b >> 7));
			fg_r = MIN<int32_t>(fg_r + ((bg_r * inv_fg_r + 127) >> 8), 255);
			fg_g = MIN<int32_t>(fg_g + ((bg_g * inv_fg_g + 127) >> 8), 255);
			fg_b = MIN<int32_t>(fg_b + ((bg_b * inv_fg_b + 127) >> 8), 255);

			shadedfg = RGB256k.All[((fg_r >> 2) << 12) | ((fg_g >> 2) << 6) | (fg_b >> 2)];
			return (fgcolor != 0) ? shadedfg : bgcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Shaded)
		{
			fgshade = (fgshade * srcalpha + 128) >> 8;
			uint32_t alpha = fgshade;
			uint32_t inv_alpha = 256 - fgshade;
			int32_t fg_r = GPalette.BaseColors[shadedfg].r;
			int32_t fg_g = GPalette.BaseColors[shadedfg].g;
			int32_t fg_b = GPalette.BaseColors[shadedfg].b;
			int32_t bg_r = GPalette.BaseColors[bgcolor].r;
			int32_t bg_g = GPalette.BaseColors[bgcolor].g;
			int32_t bg_b = GPalette.BaseColors[bgcolor].b;

			fg_r = (fg_r * alpha + bg_r * inv_alpha + 127) >> 8;
			fg_g = (fg_g * alpha + bg_g * inv_alpha + 127) >> 8;
			fg_b = (fg_b * alpha + bg_b * inv_alpha + 127) >> 8;

			shadedfg = RGB256k.All[((fg_r >> 2) << 12) | ((fg_g >> 2) << 6) | (fg_b >> 2)];
			return (alpha != 0) ? shadedfg : bgcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddClampShaded)
		{
			fgshade = (fgshade * srcalpha + 128) >> 8;
			uint32_t alpha = fgshade;
			int32_t fg_r = GPalette.BaseColors[shadedfg].r;
			int32_t fg_g = GPalette.BaseColors[shadedfg].g;
			int32_t fg_b = GPalette.BaseColors[shadedfg].b;
			int32_t bg_r = GPalette.BaseColors[bgcolor].r;
			int32_t bg_g = GPalette.BaseColors[bgcolor].g;
			int32_t bg_b = GPalette.BaseColors[bgcolor].b;

			fg_r = MIN<int32_t>(bg_r + ((fg_r * alpha + 127) >> 8), 255);
			fg_g = MIN<int32_t>(bg_g + ((fg_g * alpha + 127) >> 8), 255);
			fg_b = MIN<int32_t>(bg_b + ((fg_b * alpha + 127) >> 8), 255);

			shadedfg = RGB256k.All[((fg_r >> 2) << 12) | ((fg_g >> 2) << 6) | (fg_b >> 2)];

			return (alpha != 0) ? shadedfg : bgcolor;
		}
		else
		{
			int32_t fg_r = GPalette.BaseColors[shadedfg].r;
			int32_t fg_g = GPalette.BaseColors[shadedfg].g;
			int32_t fg_b = GPalette.BaseColors[shadedfg].b;
			int32_t bg_r = GPalette.BaseColors[bgcolor].r;
			int32_t bg_g = GPalette.BaseColors[bgcolor].g;
			int32_t bg_b = GPalette.BaseColors[bgcolor].b;

			if (BlendT::Mode == (int)BlendModes::AddClamp)
			{
				fg_r = MIN(int32_t(fg_r * srcalpha + bg_r * destalpha + 127) >> 8, 255);
				fg_g = MIN(int32_t(fg_g * srcalpha + bg_g * destalpha + 127) >> 8, 255);
				fg_b = MIN(int32_t(fg_b * srcalpha + bg_b * destalpha + 127) >> 8, 255);
			}
			else if (BlendT::Mode == (int)BlendModes::SubClamp)
			{
				fg_r = MAX(int32_t(fg_r * srcalpha - bg_r * destalpha + 127) >> 8, 0);
				fg_g = MAX(int32_t(fg_g * srcalpha - bg_g * destalpha + 127) >> 8, 0);
				fg_b = MAX(int32_t(fg_b * srcalpha - bg_b * destalpha + 127) >> 8, 0);
			}
			else if (BlendT::Mode == (int)BlendModes::RevSubClamp)
			{
				fg_r = MAX(int32_t(bg_r * srcalpha - fg_r * destalpha + 127) >> 8, 0);
				fg_g = MAX(int32_t(bg_g * srcalpha - fg_g * destalpha + 127) >> 8, 0);
				fg_b = MAX(int32_t(bg_b * srcalpha - fg_b * destalpha + 127) >> 8, 0);
			}

			shadedfg = RGB256k.All[((fg_r >> 2) << 12) | ((fg_g >> 2) << 6) | (fg_b >> 2)];
			return (fgcolor != 0) ? shadedfg : bgcolor;
		}
	}
}

template<typename BlendT, typename SamplerT>
class RectScreenDrawer8
{
public:
	static void Execute(const void *destOrg, int destWidth, int destHeight, int destPitch, const RectDrawArgs *args, PolyTriangleThreadData *thread)
	{
		using namespace TriScreenDrawerModes;

		int x0 = clamp((int)(args->X0() + 0.5f), 0, destWidth);
		int x1 = clamp((int)(args->X1() + 0.5f), 0, destWidth);
		int y0 = clamp((int)(args->Y0() + 0.5f), 0, destHeight);
		int y1 = clamp((int)(args->Y1() + 0.5f), 0, destHeight);

		if (x1 <= x0 || y1 <= y0)
			return;

		auto colormaps = args->BaseColormap();
		uint32_t srcalpha = args->SrcAlpha();
		uint32_t destalpha = args->DestAlpha();

		// Setup step variables
		float fstepU = (args->U1() - args->U0()) / (args->X1() - args->X0());
		float fstepV = (args->V1() - args->V0()) / (args->Y1() - args->Y0());
		uint32_t startU = (int32_t)((args->U0() + (x0 + 0.5f - args->X0()) * fstepU) * 0x1000000);
		uint32_t startV = (int32_t)((args->V0() + (y0 + 0.5f - args->Y0()) * fstepV) * 0x1000000);
		uint32_t stepU = (int32_t)(fstepU * 0x1000000);
		uint32_t stepV = (int32_t)(fstepV * 0x1000000);

		// Sampling stuff
		uint32_t color = args->Color();
		const uint8_t * RESTRICT translation = args->Translation();
		const uint8_t * RESTRICT texPixels = args->TexturePixels();
		uint32_t texWidth = args->TextureWidth();
		uint32_t texHeight = args->TextureHeight();

		// Setup light
		uint32_t lightshade = args->Light();
		lightshade += lightshade >> 7; // 255 -> 256
		if (SamplerT::Mode == (int)Samplers::Fuzz) lightshade = 256;

		int count = x1 - x0;

		uint32_t posV = startV;
		for (int y = y0; y < y1; y++, posV += stepV)
		{
			int coreBlock = y / 8;
			if (coreBlock % thread->num_cores != thread->core)
			{
				continue;
			}

			uint8_t *dest = ((uint8_t*)destOrg) + y * destPitch + x0;

			uint32_t posU = startU;
			for (int i = 0; i < count; i++)
			{
				uint8_t bgcolor = *dest;
				if (SamplerT::Mode == (int)Samplers::FogBoundary) color = bgcolor;
				uint8_t fgcolor = Sample8<SamplerT>(posU, posV, texPixels, texWidth, texHeight, color, translation);
				uint32_t fgshade = SampleShade8<SamplerT>(posU, posV, texPixels, texWidth, texHeight, x0 + i, y);
				*dest = ShadeAndBlend8<BlendT>(fgcolor, bgcolor, fgshade, lightshade, colormaps, srcalpha, destalpha);

				posU += stepU;
				dest++;
			}
		}
	}
};
