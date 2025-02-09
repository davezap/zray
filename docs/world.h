/*
	 ray code.
	 Daves 3D ray caster, Copy and Credit me, thanks. 

	// Used the DDSurfaceWriter base code by Jason Kresowaty to set up a window


Copyright (c) 2002-2006, David Chamberlain, I did it.
All rights reserved. And so on and so on.
email me. bobason456@hotmail.com

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, 
      this list of conditions and the following disclaimer in the documentation 
      and/or other materials provided with the distribution.
    * The name of the author may be used to endorse or promote products 
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
THE POSSIBILITY OF SUCH DAMAGE.
*/


//float LS[3][7];
double const Rad = 3.141592654 / 180;    //multiply degrees by Rad to get angle in radians
float LocalSIN[360];
float LocalCOS[360];

float iX,iY,iZ,oX,oY,oZ;	// intersection returns

float Matrix[4][4];

// Other publics
long NOBS;	// Mumber of planes 
long LSN;	// Number of lights
long NOBSt;	// Mumber of planes 
long NOBSz;	// Mumber of planes 
long LSNt;	// Number of lights

long SKYV=0;

long a;
int BGC_R,BGC_G,BGC_B;	// Amb lighting
int an_X,an_Y,an_Z;	// View Point Angle
float CamX,CamY,CamZ;	// View Point Pos

float tiA,tiB,tiAt,tiBt;

int DefRenderStp;		// Step'n

long TMS;				// fp distance from screen

bool chk_Shadow;		// On or 
bool chk_Lighting;
bool chk_Alias;
bool chk_Textures;
int chk_TextureShow;
bool chk_FastMode;
bool chk_Wire;

int tmp,tmp2;

bool Frame = false;


typedef struct Poly
{
    int pType;
    float sX;
	float sY;
	float sZ;
    float dAx;
    float dAy;
    float dAz;
    float dBx;
    float dBy;
    float dBz;
    float Surface_R;
    float Surface_G;
    float Surface_B;
    float nX;
    float nY;
    float nZ;
	long SurfaceTexture;
	float SurfaceMultW;
	float SurfaceMultH;
};


Poly Ob[100];
Poly Obt[100];
Poly Obz[100];

typedef struct Light
{
	int Type;
	bool Enabled;

	float Lx;
	float Ly;
	float Lz;
	float Colour_R;
	float Colour_G;
	float Colour_B;

	// Additional info for directional lights

	float Dx;		// Direction Vector
	float Dy;		//
	float Dz;		//
	float Cone;	// This is the cosine angle that light will spread away
					// from Dx, value 0-1
	float FuzFactor;// As percentage 0-1

	long LastPolyHit;
};	

Light LS[100];
Light LSt[100];

typedef struct cTexture
{
	long bmBitsPixel;
	long bmHeight;
	long bmPlanes;
	long bmType;
	long bmWidth;
	long bmWidthBytes;
	BYTE* bmpBuffer;
};

cTexture Texture[10];
float offR,offG,offB;
long SelTxr;