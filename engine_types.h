#ifndef ZRAY_ENGINE_TYPES_H
#define ZRAY_ENGINE_TYPES_H

#include <string>
#include <SDL3/SDL.h>

typedef unsigned int zp_size_t;
typedef unsigned int zp_screen_t;	// used for x an y coordinates in to textures and the display
typedef unsigned char BYTE;


template <typename T>
struct Colour {
	union {
		struct {
			T a, b, g, r;
		};

		T val;
	};
	

	inline Colour<T> operator*(float s) const { return { r * s, g * s, b * s }; }

	inline Colour<T>& operator*=(float t) {
		r *= t;
		g *= t;
		b *= t;
		return *this;
	}

	inline Colour<T> operator/(float s) const { s = 1 / s;  return this * s; }

	inline Colour<T>& operator/=(float t) {
		return *this *= 1 / t;
	}

	inline void fromFloatC(Colour<float> fcolour)
	{
		r = static_cast <BYTE>(fcolour.r);
		g = static_cast <BYTE>(fcolour.g);
		b = static_cast <BYTE>(fcolour.b);
		//a = static_cast <BYTE>(fcolour.a);
	}

	inline void limit_rgba() {
		if (r < 0) r = 0;
		if (g < 0) g = 0;
		if (b < 0) b = 0;
		//if (a < 0) a = 0;
		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;
		//if (a > 255) a = 255;

	}
};


// Our object structure defined either a plane or sphere. 
struct Object
{
	zp_size_t idx;
	int pType;
	Vec3 s;
	Vec3 dA;
	Vec3 dB;
	Colour<float> colour;
	Vec3 n;
	long SurfaceTexture;
	float SurfaceMultW;
	float SurfaceMultH;
	float radius_squared;

	inline float InterPlane(Vec3 o, Vec3 r)
	{
		float D = r.dot(n);

		if (D > 0) {

			Vec3 sr = s - r;
			
			Vec3 tcross = sr.cross_product(r);  // scalar triple product for plane height
			float D2 = dB.dot(tcross);

			gtl_surface_u = D2 / D;
			if ((gtl_surface_u >= 0) && (gtl_surface_u <= 1)) {
				float D3 = -dA.dot(tcross);

				gtl_surface_v = D3 / D;

				if ((gtl_surface_v >= 0) && (gtl_surface_v <= 1)) {

					float D1 = sr.dot(n);
					float L = D1 / D;
					gtl_intersect.x = r.x + L * r.x;
					gtl_intersect.y = r.y + L * r.y;
					gtl_intersect.z = r.z + L * r.z;
					return -D;
				}
			}
		}
		gtl_intersect = { 0,0,0 };
		return 0;
	}


	inline float InterSphere(const Vec3& origin, const Vec3& dir, const float& an_Y)
	{
		static bool first_hit = false;
		Vec3 intercept;

		Vec3 L = s - origin;

		Vec3 ndir = dir.unitary();

		// Assuming 'dir' is normalized, a can be 1.
		float a = ndir.dot(ndir);
		float b = 2.0 * ndir.dot(L);
		float c = L.dot(L) - radius_squared;

		float discriminant = b * b - 4 * a * c;

		if (discriminant < 0) // No real roots: the ray misses the sphere.
			return 0;

		float sqrtDiscriminant = std::sqrt(discriminant);

		// Two possible solutions for t.
		float t0 = (b - sqrtDiscriminant) / (2 * a);
		float t1 = (b + sqrtDiscriminant) / (2 * a);

		// Ensure t0 is the smaller value.
		if (t0 > t1)
			std::swap(t0, t1);

		if (t0 < 0) {
			if (t1 < 0) // Both intersections are behind the ray.
				return 0;
			t0 = t1;
		}

		// Compute the intersection point: origin + t * direction.
		intercept = origin + ndir * t0;

		gtl_intersect = intercept;

		intercept = (intercept - s);
		intercept = unit_vector(intercept);
		float qx = intercept.x, qy = intercept.y, qz = intercept.z;
		worldToLocal(intercept.x, intercept.y, intercept.z, an_Y * 0.0174533, qx, qy, qz);



		gtl_surface_u = (std::atan2(qz, qx) + Pi) / (2.0f * Pi);
		gtl_surface_v = std::acos(qy) / Pi;

		return t0;

	}

};

struct Camera
{
	Object screen;// a plane representing the screen surface.
	Vec3 fp;	// focal point behind the screen where you are sitting.
};


struct Light
{
	int Type;
	bool Enabled;
	Vec3 s;
	Colour<float> colour;
	Vec3 direction;
	float Cone;	// This is the cosine angle that light will spread away
	// from Dx, value 0-1
	float FuzFactor;// As percentage 0-1

	long LastPolyHit;	// This is for an optimization that was not used, it is being recorded.
};





struct cTexture
{
	std::wstring filename;
	SDL_Surface* surface = NULL;
	Colour<BYTE>* pixels = NULL;
	unsigned char bmBitsPixel = 0;
	unsigned char bmBytesPixel = 0;
	uint16_t bmHeight = 0;
	long bmType = 0;
	uint16_t bmWidth = 0;
	uint16_t bmWidthBytes = 0;
	unsigned char faceR = 0, faceG = 0, faceB = 0;
	bool hasAlpha = 0;
};


struct rows_cols {
	BYTE rows = 0, cols = 0;
};



#endif /* ZRAY_MATH_TYPES_H */