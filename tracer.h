#ifndef RAY_HEADER
#define RAY_HEADER

//void dbg(const wchar_t* szFormat, ...);




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
bool load_texture(cTexture& MyTexture, const wchar_t* szFileName, bool force256);
inline Colour<BYTE> get_texture_pixel(cTexture& MyTexture, zp_screen_t x, zp_screen_t y);
inline void set_plane(Object& Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline void set_plane(Object& Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline zp_size_t create_plane(float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline zp_size_t create_sphere(float X1, float Y1, float Z1, float R);
inline zp_size_t create_box(float X1, float Y1, float Z1, float wX1, float wY1, float wZ1, long SurfaceTexture, float SurfaceMultH, float SurfaceMultW);
void handle_key_event_down(SDL_Scancode code);
inline bool key_get_clear(SDL_Scancode code);
inline bool key_get(SDL_Scancode code);
void handle_key_event_up(SDL_Scancode code);
void process_inputs(void);
void hide_all_ugly_init_stuff(void);

#endif /*RAY_HEADER */