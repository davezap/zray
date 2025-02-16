#pragma once

#define fround 
#include <cmath>


const float Pi = static_cast<float>(3.1415927f);
const float Rad = static_cast<float>(0.017453292f); // Radians per degree.;


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



// Another o3-mini-high speciel. Used to keep the UV coordinates fixed to world space.
inline void worldToLocal(float worldX, float worldY, float worldZ, float rotationAngle,
	float& localX, float& localY, float& localZ) {
	// Inverse rotation about Y: rotate by -rotationAngle
	float cosA = std::cos(-rotationAngle);
	float sinA = std::sin(-rotationAngle);

	localX = cosA * worldX + sinA * worldZ;
	localZ = -sinA * worldX + cosA * worldZ;
	localY = worldY;  // Y is unchanged by a rotation around Y
}
