#ifndef ZRAY_MATH_TYPES_H
#define ZRAY_MATH_TYPES_H

// release 370/ debug 120ms ish

#include <cmath>


const float Pi = static_cast<float>(3.14159265358979323846);
const float Rad = static_cast<float>(0.01745329252); // 3.141592654 / 180;    //multiply degrees by Rad to get angle in radians

//extern thread_local float L;

extern thread_local float gtl_surface_u, gtl_surface_v, gtl_surface_u_min, gtl_surface_v_min;
extern thread_local float gtl_matrix[4][4];

// This is the replacement for the lookup tables I had previously.
#define MyCos(x) (cos(x*Rad))
#define MySin(x) (sin(x*Rad))


// Credit to whomever o3-mini-high stole this from.
inline void find_largest_factors(int c, unsigned int& a, unsigned int& b) {
	a = c;
	b = 1;

	// Start from the square root of c and find the largest factor pair
	for (int i = static_cast<int>(std::sqrt(c)); i >= 1; --i) {
		if (c % i == 0) { // Found a factor pair
			a = c / i;
			b = i;
			return;
		}
	}
}

// Vector struct and all related vector math.
struct Vec3 {

	float x, y, z;

	inline Vec3 operator-() const {
		return Vec3(-x, -y, -z);
	}
	//float operator[](int i) const { return e[i]; }
	//float& operator[](int i) { return e[i]; }


	inline Vec3 operator-(const Vec3& other) const {
		return { x - other.x, y - other.y, z - other.z };
	}

	inline void operator-=(Vec3& rhs) {
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
	}


	inline Vec3 operator+(const Vec3& other) const {
		return { x + other.x, y + other.y, z + other.z };
	}
	
	inline void operator+=(Vec3& rhs) {
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
	}

	inline Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }

	inline Vec3& operator*=(float t) {
		x *= t;
		y *= t;
		z *= t;
		return *this;
	}
	
	inline Vec3 operator/(float s) const { s = 1 / s;  return { x * s, y * s, z * s }; }

	inline Vec3& operator/=(float t) {
		return *this *= 1 / t;
	}

	inline void offset(Vec3 delta)
	{
		*this += delta;
	}

	// Dot product
	inline float dot(const Vec3& other) const {
		return x * other.x + y * other.y + z * other.z;
	}

	inline float length() const {
		return std::sqrt(length_squared());
	}

	inline float length_squared() const {
		return x * x + y * y + z * z;
	}


	inline Vec3 unitary() const {
		float len = length();
		return { x * len, y * len, z * len };
	}

	// dot product, used to get cosine angle between two vectors.
	inline float cos_angle(const Vec3& other)
	{
		//return (x * other.x + y * other.y + z * other.z) / (sqrt(x * x + y * y + z * z) * sqrt(other.x * other.x + other.y * other.y + other.z * other.z));
		return dot(other) / (length() * other.length());
	}

	inline Vec3 cross_product(Vec3& B)
	{
		return {+(y * B.z - z * B.y),
				-(x * B.z - z * B.x),
				+(x * B.y - y * B.x)};
	}

};

extern thread_local Vec3 gtl_intersect; // intersection returns


inline float CosAngle(Vec3 A, Vec3 B)
{
	return A.dot(B) / (A.length() * B.length());
	//return (A.x * B.x + A.y * B.y + A.z * B.z) / (std::sqrt(A.x * A.x + A.y * A.y + A.z * A.z) * std::sqrt(B.x * B.x + B.y * B.y + B.z * B.z));
}

inline Vec3 unit_vector(const Vec3& v) {
	float len = v.length();
	Vec3 r = { v.x / len, v.y / len, v.z / len };
	return r;
}

// Another o3-mini-high speciel.
inline void worldToLocal(float worldX, float worldY, float worldZ, float rotationAngle,
	float& localX, float& localY, float& localZ) {
	// Inverse rotation about Y: rotate by -rotationAngle
	float cosA = std::cos(-rotationAngle);
	float sinA = std::sin(-rotationAngle);

	localX = cosA * worldX + sinA * worldZ;
	localZ = -sinA * worldX + cosA * worldZ;
	localY = worldY;  // Y is unchanged by a rotation around Y
}

inline void matrix_multiply(float mat1[4][4], float mat2[4][4])
{

	float temp[4][4] = { 0 };

	for (unsigned char i = 0; i < 4; i++)
		for (unsigned char j = 0; j < 4; j++)
			for(unsigned char k = 0; k < 4; k++)
				temp[i][j] += (mat1[i][k] * mat2[k][j]);
		

	for (unsigned char i = 0; i < 4; i++)
	{
		mat1[i][0] = temp[i][0];
		mat1[i][1] = temp[i][1];
		mat1[i][2] = temp[i][2];
		mat1[i][3] = temp[i][3];
	}
}

inline void matrix_rotate(Vec3& Ang)
{
	/**/
	float rmat[4][4];

	gtl_matrix[0][0] = 1;			gtl_matrix[0][1] = 0;			gtl_matrix[0][2] = 0;	gtl_matrix[0][3] = 0;
	gtl_matrix[1][0] = 0;			gtl_matrix[1][1] = 1;			gtl_matrix[1][2] = 0;	gtl_matrix[1][3] = 0;
	gtl_matrix[2][0] = 0;			gtl_matrix[2][1] = 0;			gtl_matrix[2][2] = 1;	gtl_matrix[2][3] = 0;
	gtl_matrix[3][0] = 0;			gtl_matrix[3][1] = 0;			gtl_matrix[3][2] = 0;	gtl_matrix[3][3] = 1;

	// Y
	rmat[0][0] = MyCos(Ang.y);	rmat[0][1] = 0;		rmat[0][2] = -MySin(Ang.y);	rmat[0][3] = 0;
	rmat[1][0] = 0;				rmat[1][1] = 1;		rmat[1][2] = 0;				rmat[1][3] = 0;
	rmat[2][0] = MySin(Ang.y);	rmat[2][1] = 0;		rmat[2][2] = MyCos(Ang.y);	rmat[2][3] = 0;
	rmat[3][0] = 0;				rmat[3][1] = 0;		rmat[3][2] = 0;				rmat[3][3] = 1;
	matrix_multiply(gtl_matrix, rmat);

	// Z
	rmat[0][0] = MyCos(Ang.z);	rmat[0][1] = MySin(Ang.z);	rmat[0][2] = 0;		rmat[0][3] = 0;
	rmat[1][0] = -MySin(Ang.z);	rmat[1][1] = MyCos(Ang.z);	rmat[1][2] = 0;		rmat[1][3] = 0;
	rmat[2][0] = 0;				rmat[2][1] = 0;				rmat[2][2] = 1;		rmat[2][3] = 0;
	rmat[3][0] = 0;				rmat[3][1] = 0;				rmat[3][2] = 0;		rmat[3][3] = 1;
	matrix_multiply(gtl_matrix, rmat);
	// X
	rmat[0][0] = 1;		rmat[0][1] = 0;				rmat[0][2] = 0;				rmat[0][3] = 0;
	rmat[1][0] = 0;		rmat[1][1] = MyCos(Ang.x);	rmat[1][2] = MySin(Ang.x);	rmat[1][3] = 0;
	rmat[2][0] = 0;		rmat[2][1] = -MySin(Ang.x);	rmat[2][2] = MyCos(Ang.x);	rmat[2][3] = 0;
	rmat[3][0] = 0;		rmat[3][1] = 0;				rmat[3][2] = 0;				rmat[3][3] = 1;
	matrix_multiply(gtl_matrix, rmat);

}

inline void matrix_translate(Vec3& p)
{
	float tmat[4][4];

	tmat[0][0] = 1; tmat[0][1] = 0; tmat[0][2] = 0; tmat[0][3] = 0;
	tmat[1][0] = 0; tmat[1][1] = 1; tmat[1][2] = 0; tmat[1][3] = 0;
	tmat[2][0] = 0; tmat[2][1] = 0; tmat[2][2] = 1; tmat[2][3] = 0;
	tmat[3][0] = p.x; tmat[3][1] = p.y; tmat[3][2] = p.z; tmat[3][3] = 1;
	matrix_multiply(gtl_matrix, tmat);
}

inline void matrix_transform(Vec3& p)
{
	float outX = p.x * gtl_matrix[0][0] + p.y * gtl_matrix[1][0] + p.z * gtl_matrix[2][0] + gtl_matrix[3][0];
	float outY = p.x * gtl_matrix[0][1] + p.y * gtl_matrix[1][1] + p.z * gtl_matrix[2][1] + gtl_matrix[3][1];
	float outZ = p.x * gtl_matrix[0][2] + p.y * gtl_matrix[1][2] + p.z * gtl_matrix[2][2] + gtl_matrix[3][2];
	p.x = outX; p.y = outY; p.z = outZ;
}


#endif /* ZRAY_MATH_TYPES_H */