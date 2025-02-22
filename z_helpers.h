
#pragma once


#include <SDL3/SDL.h>
#include "z_types.h"
#include "config.h"
#include "z_helpers.h"



inline unsigned char g_keyboard_buffer[256];

bool load_texture(cTexture& MyTexture, const wchar_t* szFileName, bool force256);
inline Colour<BYTE> get_texture_pixel(cTexture& MyTexture, z_screen_t x, z_screen_t y);
void handle_key_event_down(SDL_Scancode code);
inline bool key_get_clear(SDL_Scancode code);
inline bool key_get(SDL_Scancode code);
void handle_key_event_up(SDL_Scancode code);





inline bool key_get_clear(SDL_Scancode code)
{
	if (code > 0 && code < 256) {
		if ((g_keyboard_buffer[code] & 0x02) == 0x02)
		{
			g_keyboard_buffer[code] |= 0x04;
			g_keyboard_buffer[code] &= 0xFD;
			return true;
		}
	}
	return false;
}

inline bool key_get(SDL_Scancode code)
{
	if (code > 0 && code < 256 && g_keyboard_buffer[code] & 0x01) return true;
	return false;
}



inline bool load_texture(cTexture& MyTexture, const wchar_t* szFileName, const wchar_t* szFileNameNormal)
{
	char buff[255];

	std::wstring file = ASSETS_PATH;
	file += szFileName;

	std::wcstombs(buff, file.c_str(), 255);
	SDL_Surface* bm2 = SDL_LoadBMP(buff);

	if (bm2)
	{
		SDL_Surface* bm = SDL_ConvertSurface(bm2, SDL_PIXELFORMAT_ABGR32);
		SDL_DestroySurface(bm2);
		MyTexture.pixels_colour = (Colour<BYTE>*)bm->pixels;
		MyTexture.pixels_normal = NULL;

		MyTexture.bmBytesPixel = bm->pitch / bm->w;
		MyTexture.bmBitsPixel = MyTexture.bmBytesPixel * 8;
		MyTexture.bmType = bm->format;
		MyTexture.filename = szFileName;
		MyTexture.bmWidth = bm->w;
		MyTexture.bmHeight = bm->h;
		MyTexture.bmWidthBytes = MyTexture.bmWidth * 4;
		MyTexture.mat_diffuse = { 100,0,0 };

		MyTexture.hasAlpha = false;

		if (szFileNameNormal) {
			file = ASSETS_PATH;
			file += szFileNameNormal;
			std::wcstombs(buff, file.c_str(), 255);
			SDL_Surface* bm2 = SDL_LoadBMP(buff);
			if (bm2)
			{
				Colour<BYTE>* pm = (Colour<BYTE>*)bm->pixels;
				MyTexture.pixels_normal = static_cast<Vec3*>(SDL_malloc(MyTexture.bmWidth * MyTexture.bmHeight * 3 * sizeof(float)));
				for (z_size_t a = 0; a < MyTexture.bmWidth * MyTexture.bmHeight; a++)
				{
					MyTexture.pixels_normal[a] = pm[a].toNormal();
				}
				//MyTexture.pixels_normal = ;
			}
		}

		return true;
	}



	return false;
}




// https://wiki.libsdl.org/SDL3/SDL_Scancode

inline void handle_key_event_down(SDL_Scancode code)
{
	if (code > 0 && code < 256) {
		if ((g_keyboard_buffer[code] & 0x01) == 0x00) g_keyboard_buffer[code] |= 0x01;
		if ((g_keyboard_buffer[code] & 0x04) == 0x00) g_keyboard_buffer[code] |= 0x02;
	}
}




inline void handle_key_event_up(SDL_Scancode code)
{
	if (code > 0 && code < 256)
	{
		if ((g_keyboard_buffer[code] & 0x04) == 0x04)
			g_keyboard_buffer[code] &= 0x00;
		else
			g_keyboard_buffer[code] &= 0xFE;
	}
}

