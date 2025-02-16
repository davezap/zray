#pragma once

#include "z_types.h"

void dbg(const wchar_t* szFormat, ...);
bool init_world();
void deinit_world();
void main_loop(Colour<BYTE>* src_pixels);
void render_thread(BYTE thread_num, int Xmin, int Xmax, int Ymin, int Ymax);
void trace(Vec3& o, Vec3& r, Colour<float>& pixel);
inline bool trace_light(Light* MyLS, Vec3 LSii, Object* OBJ);
void threads_clear();
void render_text_overlay(SDL_Renderer* renderer);
inline void rotate_world();
inline bool object_in_frustum(Object& Mesh);
inline bool light_in_frustum(Light& light);
inline void set_plane(Object& Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline void set_plane(Object& Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline z_size_t create_plane(float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline z_size_t create_sphere(float X1, float Y1, float Z1, float R);
inline z_size_t create_box(float X1, float Y1, float Z1, float wX1, float wY1, float wZ1, long SurfaceTexture, float SurfaceMultH, float SurfaceMultW);
void process_inputs(void);
void hide_all_ugly_init_stuff(void);
