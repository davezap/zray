#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <string>
#include <format>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include "json.hpp"

#include "z_types.h"
#include "z_helpers.h"
#include "config.h"
#include "engine.h"

using json::JSON;


#define SCREEN_LEN SCREEN_WIDTH * SCREEN_HEIGHT

// Core rendering variables. //////

Uint64 g_render_timer;			// Tracks the number of ms to render one frame
Uint64 g_render_timer_lowest = 10000;
Uint64 g_fps_timer = 0;			// Tracks time for FPS counter.
int g_fps_timer_low_pass_filter = 0;

long g_eye;						// fp distance from screen
long gro_eye;					// read only copy for threads if needed.
Colour<float> g_light_ambient;	// global additive illumination 

thread_local Vec3  gtl_intersect;	// ray plane intersection point
thread_local uv_type gtl_surface_uv;
thread_local float gtl_matrix[4][4];

// pointer to our display surface. Note that all threads
// activly write to this buffer, but if we're careful they will 
// never write to the same address.
Colour<BYTE>* g_pixels = NULL;


int gro_screen_divisor; // ... All other pixels are made the same colour.



// Multithreading support. ///////

#define MAX_THREADS 128			// I mean, how many is too many?
#ifdef _DEBUG
BYTE g_threads_requested = 1;	// It's way easyer to debug a single thread.
int g_screen_divisor = 3;	// This is the step the x/y pixel loops, 2 = half screen resolution
#else
BYTE g_threads_requested = 6;	// Standard I used for benchmarking.
int g_screen_divisor = 1;	// This is the step the x/y pixel loops, 2 = half screen resolution
#endif

size_t g_threads_allocated = 0;	// Currently allocated threads.

std::vector<std::thread> g_threads;
//std::mutex gmtx_threads_wait;
std::condition_variable gcv_threads_wait;
std::atomic<bool> gatm_threads_exit_now(false);
std::atomic<int> gatm_threads_waiting(0); // Total threads waiting

std::mutex gmtx_change_shared;	// one mutex to rule them all
std::condition_variable gcv_thread_ready;  // Signals when threads are ready
std::condition_variable gcv_threads_render_now; // Signals threads to start rendering
std::atomic<int> gatm_threads_ready_cnt(0);
bool gro_threads_render_now = false;



// Scene definitions. ////////////////

Object g_objects[100];				// All objects
z_size_t g_objects_cnt;			// ...and number off

Object g_objects_shadowers[100];	// Ob[]'s here after transforms, rotate, and frustum check.
z_size_t g_objects_shadowers_cnt;

Object* g_objects_in_view[100];		// back-face culled subset of g_objects_shadowers, 
z_size_t g_objects_in_view_cnt;	// this is what the camera can see.

Light g_lights[100];
Light g_lights_active[100];
z_size_t g_lights_cnt;				// Number of lights
z_size_t g_lights_active_cnt;

z_size_t g_textures_cnt = 0;
cTexture g_textures[10];


Vec3 g_camera_angle = { 0, 0, 0 };				// View Point Angle
Vec3 g_camera_position;				// View Point Pos
Camera g_camera;
thread_local Camera gtl_camera;



// Keyboard options.. ////////////
float g_movement_multiplier = 1;// multiplier for holding left-Shift or left-Crtl for movement.
bool g_option_shadows;
bool g_option_lighting;
bool g_option_antialias;
bool g_option_textures;



// Demo specific variables..///////
 
z_size_t g_sky_texture_idx;		// Skybox texture, if any.
Vec3 box_angle = { 0,0,0 };
z_size_t g_box_idx = 0;		// spinning cube!! 
z_size_t g_box_texture = -1;
z_size_t g_panda_ball_idx = -1;



void dbg(const wchar_t* szFormat, ...)
{
	wchar_t  szBuff[1024];
	va_list arg;
	va_start(arg, szFormat);
	_vsnwprintf_s(szBuff, sizeof(szBuff), szFormat, arg);
	va_end(arg);
	std::wcout << szBuff;
	//OutputDebugString(szBuff);
}


// called once by SDL_AppInit()
bool init_world()
{
	g_sky_texture_idx = 0;

	g_camera_position.x = 0; g_camera_position.y = 0; g_camera_position.z = -300;


	g_eye = -600;//TMS = -300;
	g_option_antialias = false;
	g_option_shadows = true;
	g_option_textures = true;
	g_option_lighting = true;

	g_light_ambient = { 1.0, 20.0, 20.0, 20.0 };

	hide_all_ugly_init_stuff();

	return true;
}

// Called once by SDL_AppQuit()
void deinit_world()
{
	threads_clear();

	for (z_size_t i = 0; i < g_textures_cnt; i++) {
		if (g_textures[i].pixels_normal) SDL_free(g_textures[i].pixels_normal);
		
	}
}


// called by SDL_AppIterate() and capped to max SCREEN_FPS_MAX
void main_loop(Colour<BYTE>* src_pixels)
{
	g_pixels = src_pixels;
	bool offscreen = false;

	g_render_timer = SDL_GetTicks();

	process_inputs();

	z_size_t a = 0;
	if (g_panda_ball_idx != -1)
	{
		if (g_objects[g_panda_ball_idx].radius_squared <= 8000)
		{
			g_objects[g_panda_ball_idx].radius_squared *= 1 + (8050 - g_objects[g_panda_ball_idx].radius_squared) / 40000;
			g_objects[g_panda_ball_idx].s.y = 100 - std::sqrtf(g_objects[g_panda_ball_idx].radius_squared);
		}
	}
	
	box_angle.z += 0.1f;
	box_angle.y += 1.0f;
	box_angle.roll360();
	///if (box_angle.x > 359) box_angle.x = box_angle.x - 360;
	///if (box_angle.y > 359) box_angle.y = box_angle.y - 360;
	Matrix44 mtrx;
	mtrx.ident();
	mtrx.rotate(box_angle);
	//Vec3 p = { -25, -25, -25 };
	//mtrx.translate(p);

	z_size_t oldcnt = g_objects_cnt;
	g_objects_cnt = g_box_idx;
	create_box({ -25, -25, -25 }, { 50, 50, 50 }, { 1.0, 0.7, 0.7, 0.7 }, g_box_texture, { 1, 1});
	g_objects_cnt = g_box_idx;
	for (a = g_box_idx; a < g_box_idx + 6; a++)
	{
		mtrx.transform(g_objects[a].s);
		mtrx.transform(g_objects[a].dA);
		mtrx.transform(g_objects[a].dB);
		mtrx.transform(g_objects[a].dAu);
		mtrx.transform(g_objects[a].dBu);
		mtrx.transform(g_objects[a].n);
		mtrx.transform(g_objects[a].nu);
		//Ob[a].n = -Ob[a].dA.normal(Ob[a].dB);
		//CalcNorm(Ob[a]);
	}
	g_objects_cnt = oldcnt;

	rotate_world();

	// Check the number of requested threads is running and start them if not.
	if (g_threads_requested != g_threads_allocated)
	{
		BYTE thd = 0;

		threads_clear();	// tier down existing thread pool.

		if (g_threads_requested < 1) g_threads_requested = 1;
		if (g_threads_requested > MAX_THREADS) g_threads_requested = MAX_THREADS;

		unsigned int cols, rows;
		find_largest_factors(g_threads_requested, cols, rows);

		// Block dimensions
		z_screen_t blockWidth = SCREEN_WIDTH / cols;
		z_screen_t blockHeight = SCREEN_HEIGHT / rows;

		// Generate coordinates for each block and stand thread up.
		thd = 0;
		for (z_screen_t row = 0; row < rows; ++row) {
			for (z_screen_t col = 0; col < cols; ++col) {
				z_screen_t x1 = col * blockWidth;
				z_screen_t y1 = row * blockHeight;
				z_screen_t x2 = (col == cols - 1) ? SCREEN_WIDTH : (x1 + blockWidth);
				z_screen_t y2 = (row == rows - 1) ? SCREEN_HEIGHT : (y1 + blockHeight);
				if (col == cols - 1) x2 -= (g_screen_divisor - 1);
				if (row == rows - 1) y2 -= (g_screen_divisor - 1);

				g_threads.emplace_back(std::thread(render_thread, thd, -(SCREEN_WIDTH / 2) + x1, -(SCREEN_WIDTH / 2) + x2, -(SCREEN_HEIGHT / 2) + y1, -(SCREEN_HEIGHT / 2) + y2));
				thd++;

			}
		}

		g_threads_allocated = g_threads.size();

		if (thd != g_threads_allocated)
		{
			// We have a problem, so just ignore it and hope for the best :)
		}

		// wait for all threads to indicate they have initialised.
		{
			std::unique_lock<std::mutex> lock(gmtx_change_shared);
			gcv_threads_wait.wait(lock, [] { return gatm_threads_waiting.load() == g_threads_allocated; });
		}
	}

	/////////////////////////////////////////
	/// Start normal render pass starts here.

	// Wait for all threads to be in ready state.
	{

		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gcv_thread_ready.wait(lock, [] { return gatm_threads_ready_cnt.load() == g_threads_allocated; });
	}

	// Make copies of all variables that are used by threads that may
	// change during the rendering pass. App usually crashes if you don't.
	gro_screen_divisor = g_screen_divisor;
	gro_eye = g_eye;

	// Command threads to start rendering
	{
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gro_threads_render_now = true;
		gcv_threads_render_now.notify_all();
	}

	// Wait for all threads to finish
	{
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gcv_thread_ready.wait(lock, [] { return gatm_threads_ready_cnt.load() == 0; });
	}

	// Command threads to stop render
	{
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gro_threads_render_now = false;
		gcv_threads_render_now.notify_all();
	}

	g_render_timer = SDL_GetTicks() - g_render_timer;

}




void render_thread(BYTE thread_num, int Xmin, int Xmax, int Ymin, int Ymax)
{

	dbg(L"[%d] Thread Start\n", thread_num);
	{
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gatm_threads_waiting.fetch_add(1); // Increment the number of threads waiting
		gcv_threads_wait.notify_all();
		//Output(L"started\n");	
	}

	//threads_waiting.fetch_add(1);

	while (!gatm_threads_exit_now.load())
	{
		{
			std::unique_lock<std::mutex> lock(gmtx_change_shared);
			gatm_threads_ready_cnt.fetch_add(1);
			gcv_thread_ready.notify_all();
			//Output(L"[%d] notify threadsReady = %d\n", thread_num, threadsReady.load());
		}

		{
			//Output(L"[%d] wait rendering\n", thread_num);
			std::unique_lock<std::mutex> lock(gmtx_change_shared);
			gcv_threads_render_now.wait(lock, [] { return gro_threads_render_now; });
		}

		if (gatm_threads_exit_now.load()) {
			//if (AltThread) Output(L"exit\n");
			return;
		}

		//Output(L"[%d] rendering...\n", thread_num);

		float SupSampSize = float(gro_screen_divisor) / 5218.9524;
		Colour<float> pixel = { 0.0f, 0.0f, 0.0f, 0.0f };		// Temp Color

		// Define out fixed FP in space
		gtl_camera = g_camera;

		int Xro = (SCREEN_WIDTH / 2);
		int Yro = (SCREEN_HEIGHT / 2);
		//tsDefRenderStp = 1;


		// ok so gtl_camera.screen.s is the center of our view port.
		// from gtl_camera.screen.s.x
		// gtl_camera.screen.dA * (Xmax - Xmin)
		// gtl_camera.screen.dB * (Ymax - Ymin)
		 
		//-Xro, -Yro // # top left


		for (float Yr = static_cast <float>(Ymin); Yr < Ymax; Yr += gro_screen_divisor)	// tsDefRenderStp
		{

			for (float Xr = static_cast <float>(Xmin); Xr < Xmax; Xr += gro_screen_divisor)
			{
				// This is the point we cast the ray through
				Vec3 pixelPos = (gtl_camera.screen.s - gtl_camera.fp) + (gtl_camera.screen.dA * Xr) + (gtl_camera.screen.dB * Yr);
				//Vec3 pixelPos = { Xr, Yr, 600 };
				pixelPos = pixelPos.unitary();
				// Reset our Point colour..
				pixel = { 0,0,0,0 };
				if (Xr == Xmin && Yr == Ymin)
				{
					pixel = { 0,0,0,0 };
				}

				trace(gtl_camera.fp, pixelPos, pixel);
				if (g_option_antialias)
				{

					// Sample around center point
					pixelPos.offset({ SupSampSize, SupSampSize, 0 });
					trace(gtl_camera.fp, pixelPos, pixel);

					pixelPos.offset({ 0, 2 * SupSampSize, 0 });
					trace(gtl_camera.fp, pixelPos, pixel);

					pixelPos.offset({ -2 * SupSampSize, 0, 0 });
					trace(gtl_camera.fp, pixelPos, pixel);

					pixelPos.offset({ 0, -2 * SupSampSize, 0 });
					trace(gtl_camera.fp, pixelPos, pixel);
					pixel /= 5;
					//ccR = ccR / 5; ccG = ccG / 5; ccB = ccB / 5;
				}

				pixel.limit_rgba();

				//g_ddsw->DrawRGBPixel(RGB(ccR, ccG, ccB));
				//g_ddsw->DrawRawPixel(g_ddsw->RGBToRaw(RGB(ccR, ccG, ccB)));

				//if((Xr + Xro) < SCREEN_WIDTH && (Yr + Yro) < SCREEN_HEIGHT)
				//{
				int sd = gro_screen_divisor;

				if (gro_screen_divisor > 1)
				{
					for (int x = 0; x < gro_screen_divisor; x++)
					{
						for (int y = 0; y < gro_screen_divisor; y++)
						{
							Colour<BYTE>* p = &g_pixels[int((y + Yr + Yro) * SCREEN_WIDTH  + (x + Xr + Xro)) % (SCREEN_LEN)];
							p->fromFloatC(pixel);
						}
					}

				}
				else {
					g_pixels[int((Yr + Yro) * SCREEN_WIDTH + (Xr + Xro))].fromFloatC(pixel);
				}

				//////////////////////////////////////////////////////
				//////////////////////////////////////////////////////
			}

		}

		//if(thread_num ==0) std::this_thread::sleep_for(std::chrono::milliseconds(100));

		{
			std::unique_lock<std::mutex> lock(gmtx_change_shared);
			gatm_threads_ready_cnt.fetch_sub(1);
			gcv_thread_ready.notify_all();
		}
		//Output(L"[%d] notified done threadsReady = %d\n", thread_num, threadsReady.load());

		{
			std::unique_lock<std::mutex> lock(gmtx_change_shared);
			gcv_threads_render_now.wait(lock, [] { return gro_threads_render_now == false; });
		}

	}
}

Vec3 lv = { 0,0,0 };

void trace(Vec3& o, Vec3& r, Colour<float>& pixel) //, Colour<float>& normal)
{

	Colour<float> tpixel = { 0.0f,0.0f,0.0f,0.0f };	// Temp RGB Values for this function
	Vec3 ii = { 0,0,0 };	// Result of Ray intersection point.
	thread_local uv_type gtl_surface_uv_min;
	float R = 10000000, lR = 0;		// Temp Len & Ang
	// Create pointer to our transformed and pruned World.
	Object* tob = g_objects_in_view[0];
	Object* MyFace = 0;



	// Find closest object : Cast ray from users eye > through screen

	z_size_t OBJ = -1;
	for (z_size_t a = 0; a < g_objects_in_view_cnt; a++)
	{
		tob = g_objects_in_view[a];

		// Calculate ray plane intersection point
		float intercept = 0;

		if (tob->pType == 1) {
			//intercept = tob->InterPlane(o, r, gtl_intersect, gtl_surface_uv);
			
			float D = r.dot(tob->n);

			if (D > 0) {
				Vec3 sr = tob->s - o;
				Vec3 tcross = sr.cross_product(r);  // scalar triple product
				float D2 = tob->dB.dot(tcross);
				if ((D2 >= 0) && (D2 <= D)) {
					float D3 = -tob->dA.dot(tcross);

					if (D3 > 0 && D3 < D) {
						gtl_surface_uv.u = D2 / D;
						gtl_surface_uv.v = D3 / D;
						intercept = sr.dot(tob->n) / D;
						gtl_intersect.x = o.x + r.x * intercept;
						gtl_intersect.y = o.y + r.y * intercept;
						gtl_intersect.z = o.z + r.z * intercept;
					}

				}
			}
			
		}
		else {
			intercept = tob->InterSphere(o, r, g_camera_angle.y, gtl_intersect, gtl_surface_uv);
			
		

		}
	sp_out:

		if (intercept)
		{

			// We're searching for the nearest surface to the observer.
			// all others are obscured.
			lR = intercept; // gtl_intersect.length_squared();				// faster than lR = sqrt((iX * iX) + (iY * iY) + (iZ * iZ));

			if (lR > 0 && lR < R)									// Compare with last
			{
				R = lR;											// This one is closer,
				ii = gtl_intersect;								//
				gtl_surface_uv_min = gtl_surface_uv;			// txU,txV are Global floats returned by
				
				// InterPlane() as floats. And represent a
				// fraction of intersection .vs. plane surface area.
				MyFace = tob;
				//OBJ = a;
			}
		}

	}


	// Now if OBJ is != -1 then we have found our final surface
	// so begin the shading pass here.
	if (MyFace)
	{
		if (g_option_lighting)
		{
			Light* MyLS = g_lights_active;

			// For each light in the scene
			//	- test if we can trace a direct path from light to surface.
			//  - if so then calculate the intensity and colour of falling on our surface
			//  - otherwise if the light cannot see surface it is casting a shadow (the absence of light)
			/*
			Vec3 pXNormal = { 0,0,0 };
			if (MyFace->SurfaceTexture > -1) {
				pXNormal = g_textures[MyFace->SurfaceTexture].get_normal(long((gtl_surface_uv_min.u * MyFace->SurfaceMultW) * g_textures[MyFace->SurfaceTexture].bmWidth), long((gtl_surface_uv_min.v * MyFace->SurfaceMultH) * g_textures[MyFace->SurfaceTexture].bmHeight));
				if (MyFace->SurfaceTexture == 1)
				{
					pXNormal = pXNormal * 1;
				}
				ii = ii + pXNormal * 1000;
			}
			*/
			Vec3 pXNormal = { 0,0,0 };
			if (MyFace->SurfaceTexture != -1 && g_textures[MyFace->SurfaceTexture].pixels_normal)
			{
				pXNormal = g_textures[MyFace->SurfaceTexture].get_normal(long((gtl_surface_uv_min.u * MyFace->SurfaceMultW) * g_textures[MyFace->SurfaceTexture].bmWidth), long((gtl_surface_uv_min.v * MyFace->SurfaceMultH) * g_textures[MyFace->SurfaceTexture].bmHeight)); Colour<BYTE> pXColor = g_textures[MyFace->SurfaceTexture].get_pixel(long((gtl_surface_uv_min.u * MyFace->SurfaceMultW) * g_textures[MyFace->SurfaceTexture].bmWidth), long((gtl_surface_uv_min.v * MyFace->SurfaceMultH) * g_textures[MyFace->SurfaceTexture].bmHeight));
			}

			for (z_size_t LSX = 0; LSX < g_lights_active_cnt; LSX++)
			{
				float F = 1; // F represents intensity from 0 to 1.  (start with one for point lights)


				Vec3 s_minus_ii = ii - MyLS->s;


				if (MyLS->Type == 2)
				{
					//continue;
					// A directional light with falloff
					//
					// First compute the angle between the light direction and Surface point.
					//F = -CosAngle(MyLS->direction - MyLS->s , s_minus_ii);
					
					F = -(MyLS->s - MyLS->direction).cos_angle(s_minus_ii);
					//F = s_minus_ii.cos_angle(MyLS->direction - MyLS->s);

					// If the poly point is within the Cone size then
					if ((F >= 1 - MyLS->Cone) && (F <= 1))
					{
						// If poly point is in Fuz edge of light then
						if (F <= 1 - (MyLS->FuzFactor * MyLS->Cone))
						{
							// Drop light intensity off to 0 at edge of Cone.
							F = (F - (1 - MyLS->Cone)) / ((1 - (MyLS->FuzFactor * MyLS->Cone)) - (1 - MyLS->Cone));
						}
						else {
							// If it's inside then full beam.
							F = 1;
						}
					}
					else {
						// Out side cone area so light gives no light.
						F = 0;
					}
				}

				// Point light (MyLS->Type==1) : radiates light in all directions so we don't need to consider cones or direction.

				// Get Cosine of light ray to surface normal (Lambert's cosine law)
				// F will = 1 if the angle is 0 and F will = 0 if it is 90deg.


				if (MyFace->pType == 2)
				{
					// For a sphere the surface normal (n) is the vector from the center to 
					// the intersection point.
					//F *= CosAngle(ii - MyFace->s, s_minus_ii);
					F *= (MyFace->s - ii).cos_angle(s_minus_ii);

				} else {

					// For planes the normal is precomputed.
					//F *= CosAngle(ii - MyFace->n, s_minus_ii);
					//F *= (ii - MyFace->n).cos_angle(s_minus_ii);
					F *= (MyFace->nu).cos_angle(s_minus_ii);

					if (MyFace->SurfaceTexture != -1 && g_textures[MyFace->SurfaceTexture].pixels_normal)
					{
						//F *= (pXNormal).cos_angle(s_minus_ii);
					}
				}

				


				// Calculate Shadows now, assuming the light actually has a potential effect on the surface.
				if (F > 0 && F <= 1)
				{

					// Now test if any other object is between the light source and the surface the ray hits.
					// This is computationally quite expensive but there are some optimizations to be had.
					//	- break from the loop on the first object we find (that we do here)
					if (MyFace->idx == 24) {
						MyFace->idx = 24;
					}
					if (g_option_shadows == true && trace_light(MyLS->s, s_minus_ii, MyFace)) goto cont;

					// Add colour components to our pix value
					F = F * 256;
					tpixel.r += (MyLS->colour.r * F);
					tpixel.g += (MyLS->colour.g * F);
					tpixel.b += (MyLS->colour.b * F);
				}
			cont:
				MyLS++;
			}


		}
		else {	// !chk_Lighting
			tpixel.r = 255;
			tpixel.g = 255;
			tpixel.b = 255;
		}


		tpixel += g_light_ambient;


		// Last step is to perform texturing of the surface point
		if (MyFace->SurfaceTexture != -1 && g_option_textures)
		{
			Colour<BYTE> pXColor = g_textures[MyFace->SurfaceTexture].get_pixel(long((gtl_surface_uv_min.u * MyFace->SurfaceMultW) * g_textures[MyFace->SurfaceTexture].bmWidth), long((gtl_surface_uv_min.v * MyFace->SurfaceMultH) * g_textures[MyFace->SurfaceTexture].bmHeight));

			pixel.r += (tpixel.r) * ((float(pXColor.r) / 255) * MyFace->colour.r);
			pixel.g += (tpixel.g) * ((float(pXColor.g) / 255) * MyFace->colour.g);
			pixel.b += (tpixel.b) * ((float(pXColor.b) / 255) * MyFace->colour.b);

		}
		else {
			// Ok, now add background illumination as a light source
			// and scale the whole thing based on the surface color

			pixel.r += (tpixel.r) * MyFace->colour.r;
			pixel.g += (tpixel.g) * MyFace->colour.g;
			pixel.b += (tpixel.b) * MyFace->colour.b;

		}
	}
	else {
		// Sky.
		pixel.g += r.z * 128; //  (+SCREEN_WIDTH / 2) / SCREEN_WIDTH;
		pixel.r += r.x * 128; //  (r.x + SCREEN_WIDTH / 2) / SCREEN_WIDTH;
		pixel.b += r.y * 128; //(r.y + SCREEN_HEIGHT / 2) / SCREEN_HEIGHT;
	}



}



inline bool trace_light(Vec3& o, Vec3& r, Object* OBJ)
{
	//float ObAYBZ,ObAXBY,ObAXBZ,ObAZBY,ObAYBX;
	Vec3 MOLS;
	float D, D1, D2, D3;
	Object* MyObb = &g_objects_shadowers[0];
	//unsigned int a = MyLS->LastPolyHit;
	//if(a>=NOBSt) a=0;
	z_size_t cnt = g_objects_shadowers_cnt;

	// Sphere test specific vars.
	float t;
	Vec3 dir = r.unitary();
	//float a = 1; // dir.dot(dir); // Assuming 'dir' is normalized, a = 1.


	while (cnt--)
	{
		Object* MyObb = &g_objects_shadowers[cnt];
		if (MyObb != OBJ)
		{
			if (MyObb->pType == 2)	// Sphere
			{

				Vec3 L = MyObb->s - o;

				float b = -2.0 * dir.dot(L);
				float c = L.dot(L) - MyObb->radius_squared;	// dA.x is radius.

				float discriminant = b * b - 4 * c; // ( b * b - 4 * a * c) Assuming 'dir' is normalized, a = 1.
				if (discriminant < 0) {
					//MyObb++; 
					continue;
				}
				 // No real roots: the ray misses the sphere.

				if (b > 0) return false;

				float sqrtDiscriminant = std::sqrt(discriminant);
				// Two possible solutions for t.
				float t0 = (-b - sqrtDiscriminant) / 2; // (2 * a)
				float t1 = (-b + sqrtDiscriminant) / 2; // (2 * a)

				// Ensure t0 is the smaller value.
				if (t0 > t1)
					std::swap(t0, t1);

				// If the smallest t is negative, then check the larger one.

				if (t0 < 0) {
					if (t1 < 0) // Both intersections are behind the ray.
					{
						//MyObb++; 
						continue;
					}
					t0 = t1;
				}

				return true;

			}
			else {	// Plane.
				
				//if (MyObb->InterPlaneTest(o, r)) return true;
							
				float D = r.dot(MyObb->nu);

				if (D > 0) {
					Vec3 sr = MyObb->s - o;
					float D1 = sr.dot(MyObb->nu) / D;
					if (D1 < 0 || D1 >= 1) continue;
					Vec3 disp = r * D1 - sr;
					uv_type uv;
					uv.u = disp.dot(MyObb->dAu) / MyObb->dA_len;
					if (uv.u < 0.0f || uv.u > 1.0f) continue;
					uv.v = disp.dot(MyObb->dBu) / MyObb->dB_len;
					if (uv.v >= 0.0f && uv.v <= 1.0f) {
						return true;
					}
				}

			}

		}
	}

	return false;

}



void threads_clear() {

	// Tell threads it's time t go.
	//gatm_threads_exit_now.store(true);
	/*
	// Release them from pending wait states.
	{
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gro_threads_render_now = false;
		gcv_threads_render_now.notify_all();
	}
	{
		// Wait for all threads to be ready
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gcv_thread_ready.wait(lock, [] { return gatm_threads_ready_cnt.load() == g_threads_allocated; });
	}
	{
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gro_threads_render_now = true;
		gcv_threads_render_now.notify_all();
	}
	*/

	// this wil be true for rendering threads.
	{	// wait for all threads to indicate they have initialised.
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gcv_threads_wait.wait(lock, [] { return gatm_threads_waiting.load() == g_threads_allocated; });
	}
	{	// Wait for all threads to be in ready state.
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gcv_thread_ready.wait(lock, [] { return gatm_threads_ready_cnt.load() == g_threads_allocated; });
	}
	gatm_threads_exit_now.store(true);
	{	// Command threads to start rendering
		std::unique_lock<std::mutex> lock(gmtx_change_shared);
		gro_threads_render_now = true;
		gcv_threads_render_now.notify_all();
	}



	// join all the threads and let them finish on their own time.
	for (auto& th : g_threads) {
		th.join();
	}

	// We only get here once all threads have exited. Lets hope we do!
	// Now reset conditions for new threads it that's what is to happen....
	g_threads.clear();
	gatm_threads_exit_now.store(false);
	gro_threads_render_now = false;
	gatm_threads_ready_cnt.store(0);
	gatm_threads_waiting.store(0);
	g_threads_allocated = 0;
}



void render_text_overlay(SDL_Renderer* renderer)
{
	int tmr = (SDL_GetTicks() - g_fps_timer)*100;
	g_fps_timer_low_pass_filter = (tmr + (g_fps_timer_low_pass_filter - tmr)/20)/100;
	g_fps_timer = SDL_GetTicks();
	if (g_render_timer < g_render_timer_lowest) g_render_timer_lowest = g_render_timer;

	SDL_SetRenderDrawColor(renderer, 0, 50, 0, 200);
	SDL_FRect r = { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = 30 };
	SDL_RenderFillRect(renderer, &r);

	SDL_SetRenderDrawColor(renderer, 0, 255, 0, 200);

	std::string debug_buffer = std::format("Move with arrows, (A)anti-aliasing={} - (T)extures={} - (S)hadows={} - (L)ights - (Z/X)Step={} (N/M)Threads={} - (1-7)Lights - (O/P) Focal distance={}", g_option_antialias, g_option_textures, g_option_shadows, g_screen_divisor, g_threads_requested, g_eye);
	SDL_RenderDebugText(renderer, 0, 5, debug_buffer.c_str());

	//debug_buffer = std::format("{}x{} : {} fps, render {} - lowest {} ms (R)reset", SCREEN_WIDTH, SCREEN_HEIGHT, 1000 / g_fps_timer_low_pass_filter, g_render_timer, g_render_timer_lowest);
	//Longer one for camera debug.
	debug_buffer = std::format("{}x{} : {} fps, render {}/{} ms - cam fp {} {} {}, s {} {} {}, A {} {} {}, B {} {} {}, n {} {} {}", SCREEN_WIDTH, SCREEN_HEIGHT, 1000 / g_fps_timer_low_pass_filter, g_render_timer, g_render_timer_lowest, g_camera.fp.x, g_camera.fp.y, g_camera.fp.z, g_camera.screen.s.x, g_camera.screen.s.y, g_camera.screen.s.z, g_camera.screen.dAu.x, g_camera.screen.dAu.y, g_camera.screen.dAu.z, g_camera.screen.dBu.x, g_camera.screen.dBu.y, g_camera.screen.dBu.z, g_camera.screen.nu.x, g_camera.screen.nu.y, g_camera.screen.nu.z);
	//Output(L"te=%d\n\r", tmr_render);

	SDL_RenderDebugText(renderer, 0, 20, debug_buffer.c_str());

}








///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rotate the world poly set and lights about CamX,CamY,CamZ
//	return new world as Obt & LSt
//	perform basic culling.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void rotate_world()
{

	// Reset camera before translation.
	set_plane(g_camera.screen, { 0, 0, -static_cast <float>(g_eye) }, { 1, 0, 0 }, { 0, 1, 0 });
	g_camera.fp = { 0, 0, 0 };
	//g_camera.fp.x 

	Matrix44 mtrx;
	
	mtrx.ident();
	mtrx.rotate(g_camera_angle);
	mtrx.translate(g_camera_position);
	mtrx.transform(g_camera.screen.s);
	mtrx.transform(g_camera.fp);


	mtrx.ident();
	mtrx.rotate(g_camera_angle);
	
	mtrx.transform(g_camera.screen.dA);
	mtrx.transform(g_camera.screen.dB);

	g_camera.screen.n = g_camera.screen.dA.cross_product(g_camera.screen.dB);
	g_camera.screen.nu = g_camera.screen.n.unitary();

	long b=0,c=0;

	for (z_size_t a = 0; a < g_objects_cnt; a++)
	{
		g_objects_shadowers[b] = g_objects[a];
		g_objects_in_view[c] = &g_objects_shadowers[b];
		c++;
		b++;
	}
	g_objects_shadowers_cnt = b;
	g_objects_in_view_cnt = c;

	b = 0;
	for (z_size_t a = 0; a < g_lights_cnt; a++)
	{
		if (g_lights[a].Enabled)
		{
			g_lights_active[b] = g_lights[a];
			b++;
		}
	}
	g_lights_active_cnt = b;

	return;
}



inline bool object_in_frustum(Object& Mesh)
{
	float sAz = Mesh.s.z + Mesh.dA.z;
	float sAy = Mesh.s.y + Mesh.dA.y;
	float sAx = Mesh.s.x + Mesh.dA.x;
	float sBz = Mesh.s.z + Mesh.dB.z;
	float sBy = Mesh.s.y + Mesh.dB.y;
	float sBx = Mesh.s.x + Mesh.dB.x;


	if ((Mesh.s.z > g_eye) || (sAz > g_eye) || (sBz > g_eye) || (sAz + sBz > g_eye))
	{
		return true;
	}
	return false;
}

inline bool light_in_frustum(Light& light)
{

	if (light.s.z > -1100)
	{
		return true;
	}
	return false;
}





inline void set_plane(Object& Mesh, Vec3 s, Vec3 da, Vec3 db) {
	Mesh.pType = 1;
	Mesh.SurfaceTexture = -1;
	Mesh.SurfaceMultH = 1;
	Mesh.SurfaceMultW = 1;
	Mesh.s = s;
	Mesh.dA = da;
	Mesh.dB = db;
	Mesh.pre_compute();
}

inline z_size_t create_plane(Vec3 s, Vec3 da, Vec3 db, Colour<float> c, z_size_t tx, uv_type tx_mult) {
	Object& Mesh = g_objects[g_objects_cnt];
	Mesh.idx = g_objects_cnt;

	set_plane(Mesh, s, da, db);
	Mesh.SurfaceTexture = tx;
	Mesh.SurfaceMultW = tx_mult.u;
	Mesh.SurfaceMultH = tx_mult.v;
	Mesh.colour = c;


	g_objects_cnt += 1;

	return g_objects_cnt - 1;

}

inline z_size_t create_sphere(Vec3 s, float radius, Colour<float> c, z_size_t tx, uv_type tx_mult)
{
	Object& Mesh = g_objects[g_objects_cnt];
	Mesh.idx = g_objects_cnt;

	Mesh.pType = 2;
	Mesh.SurfaceTexture = tx;
	Mesh.SurfaceMultW = tx_mult.u;
	Mesh.SurfaceMultH = tx_mult.v;
	Mesh.colour = c;
	Mesh.s = s;
	Mesh.radius_squared = radius * radius;

	g_objects_cnt += 1;
	return g_objects_cnt - 1;

}


inline z_size_t create_box(Vec3 s, Vec3 e, Colour<float> c, z_size_t tx, uv_type tx_mult)
{
	z_size_t StartInd = 0;

	StartInd = create_plane({ s.x + e.x, s.y + e.y, s.z }, { -e.x, 0, 0 }, { 0, -e.y, 0 }, c, tx,tx_mult);

	StartInd = create_plane({ s.x + e.x, s.y, s.z + e.z }, { -e.x, 0, 0 }, { 0, e.y, 0 }, c, tx, tx_mult);
	// Top
	StartInd = create_plane({ s.x + e.x, s.y, s.z }, { -e.x, 0, 0 }, { 0, 0, e.z }, c, tx, tx_mult);
	// Bottom
	StartInd = create_plane({ s.x, e.y + s.y, s.z }, { e.x, 0, 0 }, { 0, 0, e.z }, c, tx, tx_mult);
	StartInd = create_plane({ s.x, s.y, s.z }, { 0, e.y, 0 }, { 0, 0, e.z }, c, tx, tx_mult);
	StartInd = create_plane({ s.x + e.x, s.y + e.y, s.z }, { 0, -e.y, 0 }, { 0, 0, e.z }, c, tx, tx_mult);

	return StartInd - 5;
}








void process_inputs(void)
{
	g_movement_multiplier = 2.0f;
	if (key_get(SDL_SCANCODE_LSHIFT)) g_movement_multiplier = 5.0f;
	else if (key_get(SDL_SCANCODE_LCTRL)) g_movement_multiplier = 0.5f;

	if (key_get(SDL_SCANCODE_LEFT))
	{
		g_camera_angle.y -= 4.0f * g_movement_multiplier;
	}
	if (key_get(SDL_SCANCODE_RIGHT))
	{
		g_camera_angle.y += 4.0f * g_movement_multiplier;
	}
	if (key_get(SDL_SCANCODE_UP))
	{
		g_camera_position.x -= MySin(-g_camera_angle.y) * 15.0f * g_movement_multiplier;
		g_camera_position.z += MyCos(-g_camera_angle.y) * 15.0f * g_movement_multiplier;
	}

	if (key_get(SDL_SCANCODE_DOWN))
	{
		g_camera_position.x += MySin(-g_camera_angle.y) * 15.0f * g_movement_multiplier;
		g_camera_position.z -= MyCos(-g_camera_angle.y) * 15.0f * g_movement_multiplier;
	}

	if (key_get(SDL_SCANCODE_Y))
	{
		g_camera_angle.x -= 8.0f * g_movement_multiplier;
	}
	if (key_get(SDL_SCANCODE_H))
	{
		g_camera_angle.x += 8.0f * g_movement_multiplier;
	}


	if (g_camera_angle.x > 359) g_camera_angle.x = g_camera_angle.x - 360;
	if (g_camera_angle.y > 359) g_camera_angle.y = g_camera_angle.y - 360;
	if (g_camera_angle.z > 359) g_camera_angle.z = g_camera_angle.z - 360;
	if (g_camera_angle.x < 0) g_camera_angle.x = 360 + g_camera_angle.x;
	if (g_camera_angle.y < 0) g_camera_angle.y = 360 + g_camera_angle.y;
	if (g_camera_angle.z < 0) g_camera_angle.z = 360 + g_camera_angle.z;


	if (key_get_clear(SDL_SCANCODE_A)) g_option_antialias = !g_option_antialias;
	if (key_get_clear(SDL_SCANCODE_S)) g_option_shadows = !g_option_shadows;
	if (key_get_clear(SDL_SCANCODE_L)) g_option_lighting = !g_option_lighting;
	if (key_get_clear(SDL_SCANCODE_T)) g_option_textures = !g_option_textures;
	if (key_get_clear(SDL_SCANCODE_Z) && g_screen_divisor > 1) g_screen_divisor--;
	if (key_get_clear(SDL_SCANCODE_X) && g_screen_divisor < 11) g_screen_divisor++;
	if (key_get_clear(SDL_SCANCODE_N) && g_threads_requested > 1) g_threads_requested -= 1;
	if (key_get_clear(SDL_SCANCODE_M) && g_threads_requested < MAX_THREADS) g_threads_requested += 1;
	if (key_get_clear(SDL_SCANCODE_1)) g_lights[0].Enabled = !g_lights[0].Enabled;
	if (key_get_clear(SDL_SCANCODE_2)) g_lights[1].Enabled = !g_lights[1].Enabled;
	if (key_get_clear(SDL_SCANCODE_3)) g_lights[2].Enabled = !g_lights[2].Enabled;
	if (key_get_clear(SDL_SCANCODE_4)) g_lights[3].Enabled = !g_lights[3].Enabled;
	if (key_get_clear(SDL_SCANCODE_5)) g_lights[4].Enabled = !g_lights[4].Enabled;
	if (key_get_clear(SDL_SCANCODE_6)) g_lights[5].Enabled = !g_lights[5].Enabled;
	if (key_get_clear(SDL_SCANCODE_7)) g_lights[6].Enabled = !g_lights[6].Enabled;
	if (key_get_clear(SDL_SCANCODE_8)) g_lights[7].Enabled = !g_lights[7].Enabled;
	if (key_get_clear(SDL_SCANCODE_9)) g_lights[8].Enabled = !g_lights[8].Enabled;
	if (key_get_clear(SDL_SCANCODE_0)) g_lights[9].Enabled = !g_lights[9].Enabled;

	if (key_get(SDL_SCANCODE_Q)) lv.x -= 1;
	if (key_get(SDL_SCANCODE_W)) lv.x += 1;

	

	if (key_get_clear(SDL_SCANCODE_R)) g_render_timer_lowest = 10000;

	if (key_get(SDL_SCANCODE_O))
	{
		g_eye += 10;
	}
	if (key_get(SDL_SCANCODE_P))
	{
		g_eye -= 10;
	}

}



void fromJSON(Vec3& v, JSON& j)
{
	if(j.at(0).JSONType() == JSON::Class::Integral)
		v.x = static_cast<float>(j.at(0).ToInt());
	else
		v.x = static_cast<float>(j.at(0).ToFloat());

	if (j.at(1).JSONType() == JSON::Class::Integral)
		v.y = static_cast<float>(j.at(1).ToInt());
	else
		v.y = static_cast<float>(j.at(1).ToFloat());

	if (j.at(2).JSONType() == JSON::Class::Integral)
		v.z = static_cast<float>(j.at(2).ToInt());
	else
		v.z = static_cast<float>(j.at(2).ToFloat());
}

void fromJSONColour(Colour<float>& c, JSON& j)
{
	if (j.at(0).JSONType() == JSON::Class::Integral)
		c.r = static_cast<float>(j.at(0).ToInt());
	else
		c.r = static_cast<float>(j.at(0).ToFloat());

	if (c.r == -1) {
		c = { 1, 0.7f , 0.7f , 0.7f};
		return;
	}

	if (j.at(1).JSONType() == JSON::Class::Integral)
		c.g = static_cast<float>(j.at(1).ToInt());
	else
		c.g = static_cast<float>(j.at(1).ToFloat());
	
	if (j.at(2).JSONType() == JSON::Class::Integral)
		c.b = static_cast<float>(j.at(2).ToInt());
	else
		c.b = static_cast<float>(j.at(2).ToFloat());

	if (j.at(3).JSONType() == JSON::Class::Integral)
		c.a = static_cast<float>(j.at(3).ToInt());
	else
		c.a = static_cast<float>(j.at(3).ToFloat());
}

void fromJSONUV(uv_type& v, JSON& j)
{
	if (j.at(0).JSONType() == JSON::Class::Integral)
		v.u = static_cast<float>(j.at(0).ToInt());
	else
		v.u = static_cast<float>(j.at(0).ToFloat());

	if (j.at(1).JSONType() == JSON::Class::Integral)
		v.v = static_cast<float>(j.at(1).ToInt());
	else
		v.v = static_cast<float>(j.at(1).ToFloat());


}


void hide_all_ugly_init_stuff(void)
{
	z_size_t a = 0;
	g_lights_cnt = 0;
	std::string file = "world.json";

	std::ifstream t("world.json");
	std::string str((std::istreambuf_iterator<char>(t)),
		std::istreambuf_iterator<char>());

	JSON world = JSON::Load(str);
	//JSON lights = world.
	std::cout << world.dump();
	for (auto& j : world.at("lights").ArrayRange())
	{
		g_lights[g_lights_cnt].Type = j["type"].ToInt();
		g_lights[g_lights_cnt].Enabled = j["enabled"].ToBool();
		fromJSON(g_lights[g_lights_cnt].s, j["s"]);
		fromJSONColour(g_lights[g_lights_cnt].colour, j["colour"]);
		if (g_lights[g_lights_cnt].Type == 2) {
			fromJSON(g_lights[g_lights_cnt].direction, j["n"]);
			g_lights[g_lights_cnt].Cone = static_cast<float>(j["cone"].ToFloat());
			g_lights[g_lights_cnt].FuzFactor = static_cast<float>(j["falloff"].ToFloat());
		}
		
		g_lights_cnt++;
	}

	//g_lights_cnt = a+1 ;
	/*
	for (a = 0; a < g_lights_cnt; a++)
	{
		if(a!=9) g_lights[a].Enabled = false;
	}
	*/
	// Done.

		//walls
	float sa = 0.7f;
	g_objects_cnt = 0;

	for (auto& j : world.at("objects").ArrayRange())
	{
		int t =j["type"].ToInt();
		if (t == 1) {
			Vec3 s, da, db;
			Colour<float> c;
			z_size_t tx = j["texture"].ToInt();
			uv_type tx_uv;
			fromJSONUV(tx_uv, j["uvm"]);
			fromJSON(s, j["s"]);
			fromJSON(da, j["da"]);
			fromJSON(db, j["db"]);
			fromJSONColour(c, j["colour"]);
			
			create_plane(s, da, db, c, tx, tx_uv);
		}
		else if (t == 2) {
			Vec3 s;
			Colour<float> c;
			float radius = j["r"].ToFloat();
			z_size_t tx = j["texture"].ToInt();
			uv_type tx_uv;
			fromJSONUV(tx_uv, j["uvm"]);
			fromJSON(s, j["s"]);
			fromJSONColour(c, j["colour"]);
			int a = create_sphere(s, radius, c, tx, tx_uv);
			if (j["desc"].ToString() == "panda ball") {
				g_panda_ball_idx = a;
			}
		}
		else if (t == 3) {
			Vec3 s, e;
			Colour<float> c;
			z_size_t tx = j["texture"].ToInt();
			uv_type tx_uv;
			fromJSONUV(tx_uv, j["uvm"]);
			fromJSON(s, j["s"]);
			fromJSON(e, j["e"]);
			fromJSONColour(c, j["colour"]);
			int a = create_box(s, e, c, tx, tx_uv);

			if (j["desc"].ToString() == "vending") {
				g_objects[a].SurfaceTexture = 4;	g_objects[a].SurfaceMultW = 1.0f;	g_objects[a].SurfaceMultH = 1.0f;	// front face.
				g_objects[a].colour.b = 1.0f;		g_objects[a].colour.g = 1.0f;		g_objects[a].colour.r = 1.0f;
			}
			else if (j["desc"].ToString() == "spinbox") {
				g_box_texture = 5;
				g_box_idx = a;
			}

		}
	}
	
	
	

	// Sphere test.
	//a = create_sphere(-200, 0, 200, 100);
	//g_objects[a].colour.r = 0.5f; g_objects[a].colour.g = 0.5f; g_objects[a].colour.b = 0.5f;
	//g_objects[a].SurfaceTexture = 6; g_objects[a].SurfaceMultW = 4.0f; g_objects[a].SurfaceMultH = -4.0f;

	
	//g_box_idx = create_box(-25, 50, -25, 50, 40, 50, g_box_texture, 0.2, 0.2);


	
	// Try load the textures....
	//L"hr_wall.bmp"
	const wchar_t* texture_files[] = { L"tiles_0013_color_1k.bmp", L"stone/StoneBricksSplitface001_COL_2K.bmp", L"pexels-pixabay-268976.bmp", L"bttf.bmp", L"corkvending.ie.bmp",  L"gift.bmp" , L"panda3.bmp"};
	const wchar_t* normal_files[]  = { 0,                          L"stone/StoneBricksSplitface001_NRM_2K.bmp", 0, 0, 0, 0, 0};
	g_textures_cnt = 7;
	for (a = 0; a < g_textures_cnt; a++)
	{
		if (!load_texture(g_textures[a], texture_files[a], normal_files[a]))
		{
			if (a == g_box_texture) {
				g_box_texture = -1;
			}
			else {

				for (int b = 0; b < g_objects_cnt; b++)
				{
					if (g_objects[b].SurfaceTexture == a)
					{
						g_objects[b].SurfaceTexture = -1;
					}
				}
			}
		}
	}

}