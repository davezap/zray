#define _CRT_SECURE_NO_WARNINGS

#include <cstdlib>
#include <format>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

#include "math.h"
#include "engine_types.h"
#include "zray.h"
#include "tracer.h"



// Core rendering variables. //////

Uint64 g_render_timer;			// Tracks the number of ms to render one frame
Uint64 g_render_timer_lowest = 10000;
Uint64 g_fps_timer = 0;			// Tracks time for FPS counter.
int g_fps_timer_low_pass_filter = 0;

long g_eye;						// fp distance from screen
long gro_eye;					// read only copy for threads if needed.
Colour<BYTE> g_light_ambient;	// global additive illumination 

thread_local Vec3  gtl_intersect;	// ray plane intersection point
thread_local float gtl_surface_u, gtl_surface_v, gtl_surface_u_min, gtl_surface_v_min;
thread_local float gtl_matrix[4][4];

// pointer to our display surface. Note that all threads
// activly write to this buffer, but if we're careful they will 
// never write to the same address.
Colour<BYTE>* g_pixels = NULL;

int g_screen_divisor;	// This is the step the x/y pixel loops, 2 = half screen resolution
int gro_screen_divisor; // ... All other pixels are made the same colour.



// Multithreading support. ///////

#define MAX_THREADS 128			// I mean, how many is too many?
#ifdef _DEBUG
BYTE g_threads_requested = 1;	// It's way easyer to debug a single thread.
#else
BYTE g_threads_requested = 6;	// Standard I used for benchmarking.
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
zp_size_t g_objects_cnt;			// ...and number off

Object g_objects_shadowers[100];	// Ob[]'s here after transforms, rotate, and frustum check.
zp_size_t g_objects_shadowers_cnt;

Object* g_objects_in_view[100];		// back-face culled subset of g_objects_shadowers, 
zp_size_t g_objects_in_view_cnt;	// this is what the camera can see.

Light g_lights[100];
Light g_lights_active[100];
zp_size_t g_lights_cnt;				// Number of lights
zp_size_t g_lights_active_cnt;

zp_size_t g_textures_cnt = 0;
cTexture g_textures[10];


Vec3 g_camera_angle = { 0, 0, 0 };				// View Point Angle
Vec3 g_camera_position;				// View Point Pos
Camera g_camera;
thread_local Camera gtl_camera;



// Keyboard and options.. /////////

unsigned char g_keyboard_buffer[256];
float g_movement_multiplier = 1;// multiplier for holding left-Shift or left-Crtl for movement.
bool g_option_shadows;
bool g_option_lighting;
bool g_option_antialias;
bool g_option_textures;



// Demo specific variables..///////
 
zp_size_t g_sky_texture_idx;		// Skybox texture, if any.
bool g_vending_flicker_state = false;
zp_size_t g_vending_light_idx;
zp_size_t g_vending_object_idx;		// Vending machine front face poly.
zp_size_t g_box_idx = 0;		// spinning cube!! 
Vec3 box_angle = { 0,0,0 };
zp_size_t g_box_texture = -1;


// Things that probably don't belonw here.///////

const wchar_t* ASSETS_PATH = L"textures\\";

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
	g_screen_divisor = 3;
	g_option_lighting = true;

	g_light_ambient = { 255, 40,40,40};

	hide_all_ugly_init_stuff();

	return true;
}

// Called once by SDL_AppQuit()
void deinit_world()
{
	threads_clear();

	for (zp_size_t i = 0; i < g_textures_cnt; i++) {
		if (g_textures[i].surface) SDL_DestroySurface(g_textures[i].surface);
	}
}


// called by SDL_AppIterate() and capped to max SCREEN_FPS_MAX
void main_loop(Colour<BYTE>* src_pixels)
{
	g_pixels = src_pixels;
	bool offscreen = false;

	g_render_timer = SDL_GetTicks();

	process_inputs();

	zp_size_t a = 0;

	box_angle.x += 0.1f;
	box_angle.y += 1.0f;
	if (box_angle.x > 359) box_angle.x = box_angle.x - 360;
	if (box_angle.y > 359) box_angle.y = box_angle.y - 360;
	matrix_rotate(box_angle);

	a = g_objects_cnt;
	g_objects_cnt = g_box_idx;
	g_box_idx; create_box(-25, -25, -25, 50, 50, 50, g_box_texture, 1, 1);
	g_objects_cnt = a;
	for (a = g_box_idx; a < g_box_idx + 6; a++)
	{
		matrix_transform(g_objects[a].s);
		matrix_transform(g_objects[a].dA);
		matrix_transform(g_objects[a].dB);
		//Ob[a].n = -Ob[a].dA.normal(Ob[a].dB);
		//CalcNorm(Ob[a]);
	}

	// Flicker vending machine.
	// ** It's actually very annoying and looks like a bug.
	if(0) {
		float m = 1.0f;
		float l = 0.8f;
		int r = std::rand() % 100;
		if (r < 3 && !g_vending_flicker_state) // 1% chance it will turn off.
			g_vending_flicker_state = false;
		//else if (r < 10 && g_vending_flicker_state) // 20% if will turn back on again.
			//g_vending_flicker_state = false;

		if (g_vending_flicker_state || g_lights[9].Enabled == false)
		{
			m = 0.4f;
			l = 0.1f;
		}
		g_lights[g_vending_light_idx].colour = { 0, l, l, l };
		g_objects[g_vending_object_idx].colour = { 0, m, m, m };
	}
	else {
		float m = 0.7f;
		float l = 0.7f;
		if (g_lights[9].Enabled == false)
		{
			m = 0.4f;
			l = 0.1f;
		}
		g_lights[g_vending_light_idx].colour = { 0, l, l, l };
		g_objects[g_vending_object_idx].colour = { 0, m, m, m };
	}

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
		zp_screen_t blockWidth = SCREEN_WIDTH / cols;
		zp_screen_t blockHeight = SCREEN_HEIGHT / rows;

		// Generate coordinates for each block and stand thread up.
		thd = 0;
		for (zp_screen_t row = 0; row < rows; ++row) {
			for (zp_screen_t col = 0; col < cols; ++col) {
				zp_screen_t x1 = col * blockWidth;
				zp_screen_t y1 = row * blockHeight;
				zp_screen_t x2 = (col == cols - 1) ? SCREEN_WIDTH : (x1 + blockWidth);
				zp_screen_t y2 = (row == rows - 1) ? SCREEN_HEIGHT : (y1 + blockHeight);
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
			break;
		}

		//Output(L"[%d] rendering...\n", thread_num);

		float SupSampSize = float(gro_screen_divisor) / 3;
		Colour<float> pixel = { 0.0f, 0.0f, 0.0f, 0.0f };		// Temp Color

		// Define out fixed FP in space
		gtl_camera = g_camera;

		int Xro = (SCREEN_WIDTH / 2);
		int Yro = (SCREEN_HEIGHT / 2);
		//tsDefRenderStp = 1;


		for (float Yr = static_cast <float>(Ymin); Yr < Ymax; Yr += gro_screen_divisor)	// tsDefRenderStp
		{

			for (float Xr = static_cast <float>(Xmin); Xr < Xmax; Xr += gro_screen_divisor)
			{
				// This is the point we cast the ray through
				gtl_camera.screen.s.x = Xr; gtl_camera.screen.s.y = Yr;

				// Reset our Point colour..
				pixel = { 0,0,0,0 };

				trace(gtl_camera.fp, gtl_camera.screen.s, pixel);
				if (g_option_antialias)
				{
					// Sample around center point
					gtl_camera.screen.s.offset({ SupSampSize, SupSampSize, 0 });
					trace(gtl_camera.fp, gtl_camera.screen.s, pixel);

					gtl_camera.screen.s.offset({ 0, 2 * SupSampSize, 0 });
					trace(gtl_camera.fp, gtl_camera.screen.s, pixel);

					gtl_camera.screen.s.offset({ -2 * SupSampSize, 0, 0 });
					trace(gtl_camera.fp, gtl_camera.screen.s, pixel);

					gtl_camera.screen.s.offset({ 0, -2 * SupSampSize, 0 });
					trace(gtl_camera.fp, gtl_camera.screen.s, pixel);
					pixel /= 5;
					//ccR = ccR / 5; ccG = ccG / 5; ccB = ccB / 5;
				}

				pixel.limit_rgba();

				//g_ddsw->DrawRGBPixel(RGB(ccR, ccG, ccB));
				//g_ddsw->DrawRawPixel(g_ddsw->RGBToRaw(RGB(ccR, ccG, ccB)));

				//if((Xr + Xro) < SCREEN_WIDTH && (Yr + Yro) < SCREEN_HEIGHT)
				//{

				if (gro_screen_divisor > 1)
				{
					for (int x = 0; x < gro_screen_divisor; x++)
					{
						for (int y = 0; y < gro_screen_divisor; y++)
						{
							Colour<BYTE>* p = &g_pixels[int((y + Yr + Yro) * SCREEN_WIDTH + (x + Xr + Xro))];
							p->fromFloatC(pixel);
						}
					}

				}
				else {
					Colour<BYTE>* p = &g_pixels[int((Yr + Yro) * SCREEN_WIDTH + (Xr + Xro))];
					p->fromFloatC(pixel);
				}

				gtl_intersect.x += 1;
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



void trace(Vec3& o, Vec3& r, Colour<float>& pixel)
{

	Colour<float> tpixel = { 0.0f,0.0f,0.0f,0.0f };	// Temp RGB Values for this function
	Vec3 ii = { 0,0,0 };	// Result of Ray intersection point.
	float R = 10000000, lR = 0;		// Temp Len & Ang

	// Create pointer to our transformed and pruned World.
	Object* tob = g_objects_in_view[0];
	Object* MyFace = 0;



	// Find closest object : Cast ray from users eye > through screen

	zp_size_t OBJ = -1;
	for (zp_size_t a = 0; a < g_objects_in_view_cnt; a++)
	{
		tob = g_objects_in_view[a];

		// Calculate ray plane intersection point
		float intercept = 0;

		if (tob->pType == 1) {
			// oX - rX, oY - rY, oZ - rZ
			//intercept = InterPlaneOld(r.x, r.y, r.z, -tob->dA.x, -tob->dA.y, -tob->dA.z , -tob->dB.x, -tob->dB.y, -tob->dB.z , tob->s.x - r.x, tob->s.y - r.y, tob->s.z - r.z );
			intercept = tob->InterPlane(o, r);
		}
		else {
			intercept = tob->InterSphere(o, r, g_camera_angle.y);
		}

		if (intercept)
		{
			// We're searching for the nearest surface to the observer.
			// all others are obscured.
			lR = gtl_intersect.length_squared();				// faster than lR = sqrt((iX * iX) + (iY * iY) + (iZ * iZ));

			if (lR < R && gtl_intersect.z > 0)					// Compare with last
			{
				R = lR;								// This one is closer,
				ii = gtl_intersect;								//
				gtl_surface_u_min = gtl_surface_u; gtl_surface_v_min = gtl_surface_v;			// txU,txV are Global floats returned by
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
			Light* MyLS = &g_lights_active[0];

			// For each light in the scene
			//	- test if we can trace a direct path from light to surface.
			//  - if so then calculate the intensity and colour of falling on our surface
			//  - otherwise if the light cannot see surface it is casting a shadow (the absence of light)

			for (zp_size_t LSX = 0; LSX < g_lights_active_cnt; LSX++)
			{
				float F = 1; // F represents intensity from 0 to 1.  (start with one for point lights)

				Vec3 s_minus_ii = MyLS->s - ii;

				if (MyLS->Type == 2)
				{
					//continue;
					// A directional light with falloff
					//
					// First compute the angle between the light direction and Surface point.
					F = -CosAngle(MyLS->direction - MyLS->s , s_minus_ii);

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
					F *= CosAngle(ii - MyFace->s, s_minus_ii);
				}
				else {
					// For planes the normal is precomputed.
					F *= CosAngle(ii - MyFace->n, s_minus_ii);
				}

				// Calculate Shadows now, assuming the light actually has a potential effect on the surface.
				if (F > 0 && F <= 1)
				{

					// Now test if any other object is between the light source and the surface the ray hits.
					// This is computationally quite expensive but there are some optimizations to be had.
					//	- break from the loop on the first object we find (that we do here)
					if (g_option_shadows == true)
					{
						if (trace_light(MyLS, s_minus_ii, MyFace)) goto cont;
					}

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


		// Last step is to perform texturing of the surface point
		if (MyFace->SurfaceTexture != -1 && g_option_textures)
		{

			Colour<BYTE> pXColor = get_texture_pixel(g_textures[MyFace->SurfaceTexture], long((gtl_surface_u_min * MyFace->SurfaceMultW) * g_textures[MyFace->SurfaceTexture].bmWidth), long((gtl_surface_v_min * MyFace->SurfaceMultH) * g_textures[MyFace->SurfaceTexture].bmHeight));
			pixel.r += (tpixel.r + g_light_ambient.r) * ((float(pXColor.r) / 255) * MyFace->colour.r);
			pixel.g += (tpixel.g + g_light_ambient.g) * ((float(pXColor.g) / 255) * MyFace->colour.g);
			pixel.b += (tpixel.b + g_light_ambient.b) * ((float(pXColor.b) / 255) * MyFace->colour.b);

		}
		else {
			// Ok, now add background illumination as a light source
			// and scale the whole thing based on the surface color

			pixel.r += (tpixel.r + g_light_ambient.r) * MyFace->colour.r;
			pixel.g += (tpixel.g + g_light_ambient.g) * MyFace->colour.g;
			pixel.b += (tpixel.b + g_light_ambient.b) * MyFace->colour.b;

		}



	}
	else {

		pixel.r = 100 * (r.x + SCREEN_WIDTH / 2) / SCREEN_WIDTH;
		pixel.b = 100 * (r.y + SCREEN_HEIGHT / 2) / SCREEN_HEIGHT;
	}



}



inline bool trace_light(Light* MyLS, Vec3 LSii, Object* OBJ)
{
	//float ObAYBZ,ObAXBY,ObAXBZ,ObAZBY,ObAYBX;
	Vec3 MOLS;
	float D, D1, D2, D3;
	Object* MyObb = &g_objects_shadowers[0];
	//unsigned int a = MyLS->LastPolyHit;
	//if(a>=NOBSt) a=0;
	zp_size_t cnt = g_objects_shadowers_cnt;

	// Sphere test specific vars.
	float t;
	Vec3 dir = -LSii;
	float a = dir.dot(dir); // Assuming 'dir' is normalized, a = 1.


	while (cnt--)
	{
		if (MyObb != OBJ)
		{
			if (MyObb->pType == 2)	// Sphere
			{

				Vec3 L = MyObb->s - MyLS->s;

				float b = -2.0 * dir.dot(L);
				float c = L.dot(L) - MyObb->radius_squared;	// dA.x is radius.

				float discriminant = b * b - 4 * a * c;
				if (discriminant < 0) continue; // No real roots: the ray misses the sphere.

				if (b > 0) return false;

				float sqrtDiscriminant = std::sqrt(discriminant);
				// Two possible solutions for t.
				float t0 = (-b - sqrtDiscriminant) / (2 * a);
				float t1 = (-b + sqrtDiscriminant) / (2 * a);

				// Ensure t0 is the smaller value.
				if (t0 > t1)
					std::swap(t0, t1);

				// If the smallest t is negative, then check the larger one.

				if (t0 < 0) {
					if (t1 < 0) // Both intersections are behind the ray.
						continue;
					t0 = t1;
				}

				return true;

			}
			else {	// Plane.

				D = LSii.dot(MyObb->n);

				if (D > 0)
				{
					
					MOLS = MyObb->s - MyLS->s;

					// scalar triple product for plane height
					Vec3 tcross = MOLS.cross_product(LSii);
					D2 = MyObb->dB.dot(tcross);

					D2 /= D;


					if ((D2 >= 0) && (D2 <= 1))
					{
						
						// scalar triple product for plane width
						D3 = MyObb->dA.dot(tcross);

						// use -D instead of recomputing tcross with vectors swapped because vec(A) x vec(B) = - vec(B) x vec(A).
						// So our conditionals just need to check we're between -1 and 0 instead of 0 and 1.
						D3 /= D;

						if ((D3 >= -1) && (D3 <= 0))
						{
							
							D1 = MOLS.dot(MyObb->n) / D;

							if ((D1 >= -1) && (D1 <= 0))
							{
								return true;
							}
						}
					}

				}
			}

		}
		MyObb++;
	}

	return false;

}

void threads_clear() {

	// Tell threads it's time t go.
	gatm_threads_exit_now.store(true);

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

	debug_buffer = std::format("{}x{} : {} fps, render {} - lowest {} ms (R)reset", SCREEN_WIDTH, SCREEN_HEIGHT, 1000 / g_fps_timer_low_pass_filter, g_render_timer, g_render_timer_lowest);
	//Longer one for camera debug.
	//debug_buffer = std::format("{}x{} : {} fps, render {}/{} ms - cam fp {} {} {}, s {} {} {}, A {} {} {}, B {} {} {}, n {} {} {}", SCREEN_WIDTH, SCREEN_HEIGHT, 1000 / g_fps_timer_low_pass_filter, g_render_timer, g_render_timer_lowest, g_camera.fp.x, g_camera.fp.y, g_camera.fp.z, g_camera.screen.s.x, g_camera.screen.s.y, g_camera.screen.s.z, g_camera.screen.dA.x, g_camera.screen.dA.y, g_camera.screen.dA.z, g_camera.screen.dB.x, g_camera.screen.dB.y, g_camera.screen.dB.z, g_camera.screen.n.x, g_camera.screen.n.y, g_camera.screen.n.z);
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
	set_plane(g_camera.screen, 0, 0, -static_cast <float>(g_eye), 1, 0, 0, 0, 1, 0);
	g_camera.fp = { 0, 0, 0 };
	g_camera.screen.n = g_camera.screen.dA.cross_product(g_camera.screen.dB);
	g_camera.screen.s.x = g_camera.screen.s.x; g_camera.screen.s.y = g_camera.screen.s.y; g_camera.screen.s.z = g_camera.screen.s.z;


	matrix_rotate(g_camera_angle);
	long b=0,c=0;

	for(zp_size_t a=0;a<g_objects_cnt;a++)
	{
		g_objects_shadowers[b] = g_objects[a];
		g_objects_shadowers[b].s = g_objects_shadowers[b].s - g_camera_position;
	
		matrix_transform(g_objects_shadowers[b].s);

		// Kill polys that are further then 2000 units away.
		
		//if(g_objects_shadowers[b].s.length() < 10000)
		//{
			if(g_objects_shadowers[b].pType == 1)
			{
				// recompute plane normals.
				matrix_transform(g_objects_shadowers[b].dA);
				matrix_transform(g_objects_shadowers[b].dB);
				g_objects_shadowers[b].n = g_objects_shadowers[b].dA.cross_product(g_objects_shadowers[b].dB);
			}

			if (object_in_frustum(g_objects_shadowers[b]) == true)
			{
				//// remove polygons that we cant see.
				float ca = CosAngle(g_objects_shadowers[b].s + g_objects_shadowers[b].n, g_objects_shadowers[b].s);
				if (g_objects_shadowers[b].pType == 2 || ca >= 0.0f)
				{
					g_objects_in_view[c] = &g_objects_shadowers[b];	
					c++;
				}

					
			}	
			b++;
		//}
	}
	g_objects_shadowers_cnt = b;
	g_objects_in_view_cnt = c;

	b = 0;
	for(zp_size_t a=0;a<g_lights_cnt;a++)
	{
		if(g_lights[a].Enabled)
		{
			g_lights_active[b] = g_lights[a];
			g_lights_active[b].s -= g_camera_position;
			
			matrix_transform(g_lights_active[b].s);

			//if(g_lights_active[b].s.length() <2000)
			//{
				//if(light_in_frustum(g_lights_active[b]))
				//{
					if(g_lights_active[b].Type == 2)
					{
						g_lights_active[b].direction -= g_camera_position;
						matrix_transform(g_lights_active[b].direction);
					}
					b++;
				//}
			//}
		}
	}
	g_lights_active_cnt = b;
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




bool load_texture(cTexture& MyTexture, const wchar_t* szFileName, bool force256)
{
	char buff[255];

	std::wstring file = ASSETS_PATH;
	file += szFileName;
	std::wcstombs(buff, file.c_str(),255);
	SDL_Surface* bm2 = SDL_LoadBMP(buff);

	if (bm2)
	{
		SDL_Surface* bm = SDL_ConvertSurface(bm2, SDL_PIXELFORMAT_ABGR32);
		SDL_DestroySurface(bm2);
		MyTexture.surface = bm2;
		MyTexture.pixels = (Colour<BYTE>*)bm->pixels;
		MyTexture.bmBytesPixel = bm->pitch / bm->w;
		MyTexture.bmBitsPixel = MyTexture.bmBytesPixel * 8;
		MyTexture.bmType = bm->format;
		MyTexture.filename = szFileName;		
		MyTexture.bmWidth = bm->w;
		MyTexture.bmHeight = bm->h;
		MyTexture.bmWidthBytes = MyTexture.bmWidth * 4;


		MyTexture.hasAlpha = false;

		return true;
	}

	return false;
}

inline Colour<BYTE> get_texture_pixel(cTexture &MyTexture, zp_screen_t x, zp_screen_t y)
{
	x %= MyTexture.bmWidth;
	y %= MyTexture.bmHeight;
	return MyTexture.pixels[x + y * MyTexture.bmWidth];
}


inline void set_plane(Object& Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3)
{

	Mesh.pType = 1;
	Mesh.SurfaceTexture = -1;
	Mesh.SurfaceMultH = 1;
	Mesh.SurfaceMultW = 1;
	Mesh.s = { X1, Y1 , Z1 };
	Mesh.dA = { X2, Y2, Z2 };
	Mesh.dB = { X3, Y3 ,Z3 };
}

inline zp_size_t create_plane(float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3)
{
	Object& Mesh = g_objects[g_objects_cnt];
	Mesh.idx = g_objects_cnt;

	set_plane(Mesh, X1, Y1, Z1, X2, Y2, Z2, X3, Y3, Z3);

	g_objects_cnt += 1;
	return g_objects_cnt - 1;

	//CalcNorm(Mesh);
	//Mesh.n = Mesh.dA.normal(Mesh.dB);
}

inline zp_size_t create_sphere(float X1, float Y1, float Z1, float R)
{
	Object& Mesh = g_objects[g_objects_cnt];
	Mesh.idx = g_objects_cnt;

	Mesh.pType = 2;
	Mesh.SurfaceTexture = -1;
	Mesh.SurfaceMultH = 1;
	Mesh.SurfaceMultW = 1;
	Mesh.s = { X1, Y1, Z1 };
	Mesh.radius_squared = R * R;

	g_objects_cnt += 1;
	return g_objects_cnt - 1;

}


inline zp_size_t create_box(float X1, float Y1, float Z1, float wX1, float wY1, float wZ1, long SurfaceTexture, float SurfaceMultH, float SurfaceMultW)
{
	zp_size_t StartInd = 0;

	StartInd = create_plane(X1 + wX1, Y1 + wY1, Z1, -wX1, 0, 0, 0, -wY1, 0);
	g_objects[StartInd].colour.r = 1; g_objects[StartInd].colour.g = 1; g_objects[StartInd].colour.b = 1;
	g_objects[StartInd].SurfaceTexture = SurfaceTexture; g_objects[StartInd].SurfaceMultH = SurfaceMultH; g_objects[StartInd].SurfaceMultW = SurfaceMultW;

	StartInd = create_plane(X1 + wX1, Y1, Z1 + wZ1, -wX1, 0, 0, 0, wY1, 0);
	g_objects[StartInd].colour.r = 1; g_objects[StartInd].colour.g = 1; g_objects[StartInd].colour.b = 1;
	g_objects[StartInd].SurfaceTexture = SurfaceTexture; g_objects[StartInd].SurfaceMultH = SurfaceMultH; g_objects[StartInd].SurfaceMultW = SurfaceMultW;

	// Top
	StartInd = create_plane(X1 + wX1, Y1, Z1, -wX1, 0, 0, 0, 0, wZ1);
	g_objects[StartInd].colour.r = 1; g_objects[StartInd].colour.g = 1; g_objects[StartInd].colour.b = 1;
	g_objects[StartInd].SurfaceTexture = SurfaceTexture; g_objects[StartInd].SurfaceMultH = SurfaceMultH; g_objects[StartInd].SurfaceMultW = SurfaceMultW;

	// Bottom
	StartInd = create_plane(X1, wY1 + Y1, Z1, wX1, 0, 0, 0, 0, wZ1);
	g_objects[StartInd].colour.r = 1; g_objects[StartInd].colour.g = 1; g_objects[StartInd].colour.b = 1;
	g_objects[StartInd].SurfaceTexture = SurfaceTexture; g_objects[StartInd].SurfaceMultH = SurfaceMultH; g_objects[StartInd].SurfaceMultW = SurfaceMultW;

	StartInd = create_plane(X1, Y1, Z1, 0, wY1, 0, 0, 0, wZ1);
	g_objects[StartInd].colour.r = 1; g_objects[StartInd].colour.g = 1; g_objects[StartInd].colour.b = 1;
	g_objects[StartInd].SurfaceTexture = SurfaceTexture; g_objects[StartInd].SurfaceMultH = SurfaceMultH; g_objects[StartInd].SurfaceMultW = SurfaceMultW;

	StartInd = create_plane(X1 + wX1, Y1 + wY1, Z1, 0, -wY1, 0, 0, 0, wZ1);
	g_objects[StartInd].colour.r = 1; g_objects[StartInd].colour.g = 1; g_objects[StartInd].colour.b = 1;
	g_objects[StartInd].SurfaceTexture = SurfaceTexture; g_objects[StartInd].SurfaceMultH = SurfaceMultH; g_objects[StartInd].SurfaceMultW = SurfaceMultW;

	return StartInd - 5;
}





// https://wiki.libsdl.org/SDL3/SDL_Scancode

void handle_key_event_down(SDL_Scancode code)
{
	if (code > 0 && code < 256) {
		if ((g_keyboard_buffer[code] & 0x01) == 0x00) g_keyboard_buffer[code] |= 0x01;
		if ((g_keyboard_buffer[code] & 0x04) == 0x00) g_keyboard_buffer[code] |= 0x02;
	}
}

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


void handle_key_event_up(SDL_Scancode code)
{
	if (code > 0 && code < 256)
	{
		if ((g_keyboard_buffer[code] & 0x04) == 0x04)
			g_keyboard_buffer[code] &= 0x00;
		else
			g_keyboard_buffer[code] &= 0xFE;
	}
}


void process_inputs(void)
{
	g_movement_multiplier = 0.5f;
	if (key_get(SDL_SCANCODE_LSHIFT)) g_movement_multiplier = 2.0f;
	else if (key_get(SDL_SCANCODE_LCTRL)) g_movement_multiplier = 0.3f;

	if (key_get(SDL_SCANCODE_LEFT))
	{
		g_camera_angle.y += 4.0f * g_movement_multiplier;
	}
	if (key_get(SDL_SCANCODE_RIGHT))
	{
		g_camera_angle.y -= 4.0f * g_movement_multiplier;
	}
	if (key_get(SDL_SCANCODE_UP))
	{
		g_camera_position.z += MyCos(g_camera_angle.y) * 15.0f * g_movement_multiplier;
		g_camera_position.x -= MySin(g_camera_angle.y) * 15.0f * g_movement_multiplier;
	}

	if (key_get(SDL_SCANCODE_DOWN))
	{
		g_camera_position.z -= MyCos(g_camera_angle.y) * 15.0f * g_movement_multiplier;
		g_camera_position.x += MySin(g_camera_angle.y) * 15.0f * g_movement_multiplier;
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


void hide_all_ugly_init_stuff(void)
{
	zp_size_t a = 0;


	
	/*


	load_texture(g_textures[1], L"hr_wall.bmp", false); // walls

	load_texture(g_textures[2], L"pexels-pixabay-268976.bmp", false); // ground
	load_texture(g_textures[3], L"wood.bmp", false); // bench
	load_texture(g_textures[4], L"bttf.bmp", false); // bench
	g_sky_texture_idx = 155;
	load_texture(g_textures[5], L"corkvending.ie.bmp", false); // coke
	load_texture(g_textures[6], L"gift.bmp", false); // gift wrap

	load_texture(g_textures[7], L"panda3.bmp", false);


	g_textures_cnt = 8;
	*/
	

	// Lights...

	a = 0; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = -0.0f;  g_lights[a].s.y = -60.0f;  g_lights[a].s.z = 0.0f;
	g_lights[a].colour.r = 0.8f; g_lights[a].colour.g = 0.8f; g_lights[a].colour.b = 0.6f;

	a = 1; g_lights[a].Type = 2;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = -0.0f;  g_lights[a].s.y = -80.0f; g_lights[a].s.z = 0.0f;
	g_lights[a].colour.r = 0.6f; g_lights[a].colour.g = 0.0f; g_lights[a].colour.b = 0.0f;
	g_lights[a].direction.x = 0.0f; g_lights[a].direction.y = 50.0f;  g_lights[a].direction.z = 400.0f; g_lights[a].Cone = 0.3f;
	g_lights[a].FuzFactor = 0.4f;

	a = 2; g_lights[a].Type = 2;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = -0.0f; g_lights[a].s.y = -80.0f;  g_lights[a].s.z = 0.0f; g_lights[a].colour.r = 0.0f; g_lights[a].colour.g = 0.0f; g_lights[a].colour.b = 0.6f;
	g_lights[a].direction.x = 400.0f; g_lights[a].direction.y = 50.0f;  g_lights[a].direction.z = 400.0f; g_lights[a].Cone = 0.3f;
	g_lights[a].FuzFactor = 0.4f;
	a = 3; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = -380.0f; g_lights[a].s.y = -50.0f;  g_lights[a].s.z = 380.0f; g_lights[a].colour.r = 0.2f; g_lights[a].colour.g = 0.2f; g_lights[a].colour.b = 0.2f;
	a = 4; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = 380.0f; g_lights[a].s.y = -50.0f;  g_lights[a].s.z = 280.0f; g_lights[a].colour.r = 0.2f; g_lights[a].colour.g = 0.2f; g_lights[a].colour.b = 0.2f;
	a = 5; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = -380.0f; g_lights[a].s.y = -50.0f;  g_lights[a].s.z = -380.0f; g_lights[a].colour.r = 0.2f; g_lights[a].colour.g = 0.2f; g_lights[a].colour.b = 0.3f;
	a = 6; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = 380.0f; g_lights[a].s.y = -50.0f;  g_lights[a].s.z = -380.0f; g_lights[a].colour.r = 0.5f; g_lights[a].colour.g = 0.5f; g_lights[a].colour.b = 0.2f;
	a = 6; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = 380.0f; g_lights[a].s.y = -50.0f;  g_lights[a].s.z = -380.0f; g_lights[a].colour.r = 0.2f; g_lights[a].colour.g = 0.5f; g_lights[a].colour.b = 0.2f;
	a = 7; g_lights[a].Type = 2;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = 0.0f; g_lights[a].s.y = -90.0f;  g_lights[a].s.z = -900.0f; g_lights[a].colour.r = 0.8f; g_lights[a].colour.g = 0.4f; g_lights[a].colour.b = 0.4f;
	g_lights[a].direction.x = 0.0f; g_lights[a].direction.y = 90.0f;  g_lights[a].direction.z = -1100.0f; g_lights[a].Cone = 0.5f;
	g_lights[a].FuzFactor = 0.6f;
	a = 8; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = 500.0f; g_lights[a].s.y = -50.0f;  g_lights[a].s.z = -1300.0f; g_lights[a].colour.r = 0.5f; g_lights[a].colour.g = 0.5f; g_lights[a].colour.b = 0.5f;

	a = 9; g_lights[a].Type = 1;
	g_lights[a].Enabled = true;
	g_lights[a].s.x = 350.0f; g_lights[a].s.y = 10.0f;  g_lights[a].s.z = 280.0f;
	g_lights[a].colour.r = 0.8f; g_lights[a].colour.g = 0.8f; g_lights[a].colour.b = 0.8f;
	g_lights[a].direction.x = 350.0f; g_lights[a].direction.y = 10.0f;  g_lights[a].direction.z = 299.0f; g_lights[a].Cone = 0.9f;
	g_vending_light_idx = 9;


	g_lights_cnt = a + 1;
	/*
	for (a = 0; a < g_light_cnt; a++)
	{
		if(a!=1) g_lights[a].Enabled = false;
	}
	*/

	// Done.

		//walls
	float sa = 0.7f;
	g_objects_cnt = 0;

	a = create_plane(600, -100, -2800, 0, 0, 3200, -1000, 0, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // ceiling
	g_objects[a].SurfaceTexture = 0; g_objects[a].SurfaceMultW = 5.3f; g_objects[a].SurfaceMultH = 1.305f;

	a = create_plane(-400, 100, -2800, 0, 0, 3200, 1000, 0, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // floor
	g_objects[a].SurfaceTexture = 2; g_objects[a].SurfaceMultW = 4.0f; g_objects[a].SurfaceMultH = 3.0f;

	a = create_plane(-400, -100, -400, 0, 0, 800, 0, 200, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // wall left
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(400, -100, 400, 0, 0, -800, 0, 200, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // wall right
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(-400, -100, 400, 800, 0, 0, 0, 200, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // wall front.
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(400, -100, -400, -300, 0, 0, 0, 200, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // back right
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 0.9f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(-100, -100, -400, -300, 0, 0, 0, 200, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // back left
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 0.9f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(398, -50, 250, 0, 0, -141, 0, 100, 0);  g_objects[a].colour.r = sa; g_objects[a].colour.g = sa; g_objects[a].colour.b = sa; // painting.
	g_objects[a].SurfaceTexture = 3; g_objects[a].SurfaceMultW = 1.0f; g_objects[a].SurfaceMultH = 1.0f;

	// plinth.
	a = create_box(-25, 50, -25, 50, 40, 50, 0.2, 0.2, 0.2);
	for (int b = a; b < a+6; b++)
	{
		g_objects[b].SurfaceTexture = -1; g_objects[b].colour.b = 0.8f; g_objects[b].colour.g = 0.8f; g_objects[b].colour.r = 0.8f;
	}

	// vending machine.	
	a = create_box(300, -80, 300, 100, 180, 100, 3, 1, 1);
	g_objects[a].SurfaceTexture = 4;	g_objects[a].SurfaceMultW = 1.0f;	g_objects[a].SurfaceMultH = 1.0f;	// front face.
	g_objects[a].colour.b = 1.0f;		g_objects[a].colour.g = 1.0f;		g_objects[a].colour.r = 1.0f;
	g_objects[a + 1].SurfaceTexture = -1; g_objects[a + 1].colour.b = 0.2f; g_objects[a + 1].colour.g = 0.2f; g_objects[a + 1].colour.r = 0.2f;
	g_objects[a + 2].SurfaceTexture = -1; g_objects[a + 2].colour.b = 0.2f; g_objects[a + 2].colour.g = 0.2f; g_objects[a + 2].colour.r = 0.2f;
	g_objects[a + 3].SurfaceTexture = -1; g_objects[a + 3].colour.b = 0.2f; g_objects[a + 3].colour.g = 0.2f; g_objects[a + 3].colour.r = 0.2f;
	g_objects[a + 4].SurfaceTexture = -1; g_objects[a + 4].colour.b = 0.2f; g_objects[a + 4].colour.g = 0.2f; g_objects[a + 4].colour.r = 0.2f;
	g_objects[a + 5].SurfaceTexture = -1;  g_objects[a + 5].colour.b = 0.2f; g_objects[a + 5].colour.g = 0.2f; g_objects[a + 5].colour.r = 0.2f;




	a = create_plane(-100, -100, -1000, 0, 0, 600, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(100, -100, -400, 0, 0, -600, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(400, -100, -1200, -800, 0, 0, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 2.4f; g_objects[a].SurfaceMultH = 1.0f;



	a = create_plane(-400, -100, -1000, 300, 0, 0, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(100, -100, -1000, 500, 0, 0, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 1.5f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(-400, -100, -1200, 0, 0, 200, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 0.6f; g_objects[a].SurfaceMultH = 1.0f;



	a = create_plane(400, -100, -2800, 0, 0, 1600, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 4.8f; g_objects[a].SurfaceMultH = 1.0f;

	a = create_plane(600, -100, -1000, 0, 0, -1800, 0, 200, 0);  g_objects[a].colour.r = 1.0f; g_objects[a].colour.g = 1.0f; g_objects[a].colour.b = 1.0f;
	g_objects[a].SurfaceTexture = 1; g_objects[a].SurfaceMultW = 5.4f; g_objects[a].SurfaceMultH = 1.0f;

	// Sphere test.
	a = create_sphere(-200, 0, 200, 100);
	g_objects[a].colour.r = 0.5f; g_objects[a].colour.g = 0.5f; g_objects[a].colour.b = 0.5f;
	g_objects[a].SurfaceTexture = 6; g_objects[a].SurfaceMultW = 4.0f; g_objects[a].SurfaceMultH = -4.0f;

	g_box_texture = 5;
	g_box_idx = create_box(-25, 50, -25, 50, 40, 50, g_box_texture, 0.2, 0.2);
	
	// Try load the textures....

	const wchar_t* texture_files[] = { L"tiles_0013_color_1k.bmp", L"hr_wall.bmp", L"pexels-pixabay-268976.bmp", L"bttf.bmp", L"corkvending.ie.bmp",  L"gift.bmp" , L"panda3.bmp" };
	g_textures_cnt = 7;
	for (a = 0; a < g_textures_cnt; a++)
	{
		if (!load_texture(g_textures[a], texture_files[a], false))
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