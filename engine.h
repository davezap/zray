#pragma once

#include "z_types.h"

void dbg(const wchar_t* szFormat, ...);
bool init_world();
void deinit_world();
void main_loop(Colour<BYTE>* src_pixels);
void render_thread(BYTE thread_num, int Xmin, int Xmax, int Ymin, int Ymax);
void trace(Vec3& o, Vec3& r, Colour<float>& pixel);
inline bool trace_light(Vec3& o, Vec3& r, Object* OBJ);
void threads_clear();
void render_text_overlay(SDL_Renderer* renderer);
inline void rotate_world();
inline bool object_in_frustum(Object& Mesh);
inline bool light_in_frustum(Light& light);
inline void set_plane(Object& Mesh, Vec3 s, Vec3 da, Vec3 db);
inline z_size_t create_plane(Vec3 s, Vec3 da, Vec3 db, Colour<float> c, z_size_t tx, uv_type tx_mult);
inline z_size_t create_sphere(Vec3 s, float radius, Colour<float> c, z_size_t tx, uv_type tx_mult);
inline z_size_t create_box(Vec3 s, Vec3 e, Colour<float> c, z_size_t tx, uv_type tx_mult);
void process_inputs(void);
void hide_all_ugly_init_stuff(void);
