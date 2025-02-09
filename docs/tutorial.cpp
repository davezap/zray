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



#define WIN32_LEAN_AND_MEAN


// Typical header setup.
///#include <Afxwin.h>
#include <windows.h>
#include <math.h>

#include <ddraw.h>
#include "ddsurface.h"
#include "world.h"

#include <Stdio.h>

// Global variables for instance and window
HINSTANCE g_hInst = NULL;
HWND g_hwndMain = NULL;

///HBITMAP hBitmap;

int c=0;
// Global variable to point to the DDSurfaceWriter
DDSurfaceWriter *g_ddsw = NULL;


// Function prototypes
inline void DrawScreen();
inline void DrawScreenB();
bool OnIdle(LONG lCount);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ErrorMessage(LPCSTR lpText, HRESULT hResult);

// World Function prototypes
inline void SetUpWorld();
inline void SetUpWorldB();
inline void CreatePlaneEx(Poly &Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3);
inline void CalcNorm(Poly &Mesh);

inline void CreateBox(long& StartInd, float X1, float Y1, float Z1, float wX1, float wY1, float wZ1);
inline float InterPlane(float a11, float a21, float a31, float a12, float a22, float a32, float a13, float a23, float a33, float b1, float b2, float b3);
inline float InterPlaneEx(float a11, float a21, float a31, float a12, float a22, float a32, float a13, float a23, float a33, float b1, float b2, float b3);
inline float CosAngle(float AX, float AY, float AZ, float bx, float by, float bz);

inline void MatMult(float mat1[4][4], float mat2[4][4]);
inline void RotatePntExB(int AngX, int AngY, int AngZ);
inline void Transform(float &pX, float &pY, float &pZ);
inline void Translate(float Xt, float Yt, float Zt);
inline void Render();
inline void RenderW();
inline void RenderTF();

//inline void TraceA(float Xr,float Yr, float &ccR, float &ccG, float &ccB);
inline void TraceA(float &rX,float &rY, float &rZ, float &oX, float &oY, float &oZ, float &ccR, float &ccG, float &ccB);
inline bool WhatOb(Light &MyLS, float LSiix,float LSiiy,float LSiiz,long &OBJ);
inline void TraceAFast(float &rX,float &rY, float &rZ, float &oX, float &oY, float &oZ, float &ccR, float &ccG, float &ccB);

inline void ClearScreen();

inline float MyCos(int angle);
inline float MySin(int angle);

bool LoadTexture(cTexture &MyTexture, LPTSTR szFileName);
bool LoadBitmapFromBMPFile( LPTSTR szFileName, HBITMAP *phBitmap,HPALETTE *phPalette );
inline RGBQUAD GetPixel(cTexture &MyTexture, long x, long y);
inline void DrawTexture();


inline RotateWorld();
inline bool FalcTest(Poly &Mesh);
inline void MeshLights();
inline void MeshLightsB();



// Entry point.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  // Store the instance handle for future calls.
  g_hInst = hInstance;

  // Window class for the app's main window.
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.hInstance = g_hInst;
  wcex.lpszClassName = "MainWindow";
  wcex.lpfnWndProc = (WNDPROC) MainWindowProc;
  wcex.style = 0;
  wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wcex.hIconSm = NULL;
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.lpszMenuName = NULL;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hbrBackground = (HBRUSH) GetStockObject(NULL_BRUSH);

  // Register the window class.
  if (!RegisterClassEx(&wcex))
  {
    MessageBox(NULL, "RegisterClassEx failed.", NULL, MB_OK);
    return 1;
  }

  // Create and display the app's main window
  HWND g_hwndMain = CreateWindowEx(WS_EX_TOPMOST, "MainWindow", "Tutorial", WS_VISIBLE | WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
  if (!g_hwndMain)
  {
    MessageBox(NULL, "CreateWindowEx failed.", NULL, MB_OK);
    return 1;
  }
  ShowWindow(g_hwndMain, nCmdShow);
  UpdateWindow(g_hwndMain);

  LPDIRECTDRAW lpdd;
  HRESULT hr = DirectDrawCreate(NULL, &lpdd, NULL);
  if (FAILED(hr))
  {
    ErrorMessage("DirectDrawCreate failed.", hr);
    return 1;
  }

  // Obtain full-screen cooperation with DirectDraw.
  if (FAILED(hr = lpdd->SetCooperativeLevel(g_hwndMain, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN)))
  {
    ErrorMessage("SetCooperativeLevel failed.", hr);
    lpdd->Release();
    return 1;
  }

  // Set the display mode.             

  if (FAILED(hr = lpdd->SetDisplayMode(640, 480, 24)))
  {
    // Try to use 32bpp instead.
    if (FAILED(hr = lpdd->SetDisplayMode(640, 480, 32)))
    {
      ErrorMessage("SetDisplayMode failed.", hr);
      lpdd->Release();
      return 1;
    }
  }

  // Interface pointer for the primary surface.
  LPDIRECTDRAWSURFACE lpddsPrimary;
  // Descriptor for the primary surface.
  DDSURFACEDESC ddsd;

  // Initialization.
  ZeroMemory(&ddsd, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

  // Create the primary surface.
  if (FAILED(hr = lpdd->CreateSurface(&ddsd, &lpddsPrimary, NULL)))
  {
    ErrorMessage("CreateSurface failed.", hr);
    lpdd->RestoreDisplayMode();
    lpdd->Release();
    return 1;
  }

  // Create a new DDSurfaceWriter.
  g_ddsw = new DDSurfaceWriter;
  
  // Initialize the DDSurfaceWriter with our surface.
  if (FAILED(hr = g_ddsw->Initialize(lpddsPrimary, lpdd)))
  {
    ErrorMessage("Initialize surface writer failed.", hr);
    lpdd->RestoreDisplayMode();
    lpdd->Release();
    delete g_ddsw;
    return 1;
  }

  SetUpWorld();
  // Draw the display.
  DrawScreen();

  // Main message loop.
  MSG msg;


	while(true)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else if(IsWindowEnabled(g_hwndMain))
		{
			if(chk_Wire) ClearScreen();
			DrawScreen();
			//if(!chk_Wire) RenderW();
		}

		else WaitMessage();
	}

  //Broken from infinate loop to handle WM_QUIT message

  // Restore the user's original screen mode.
  lpdd->RestoreDisplayMode();

  // Release the primary surface and the DirectDraw object.
  lpddsPrimary->Release();
  lpdd->Release();
//  ErrorMessage("The end.", NULL);

  return msg.wParam;

}



long multFIX(long fix_1,long fix_2)
{
	return (fix_1 * fix_2) >> 8;
}

void intFIX(void)
{
	long fix_1 = (long)(23.45 * 256);
	long fix_2 = (long)(1423.13 * 256);
	long fix_3 = 0;

	for(long a=0;a<100000000;a++)
	{
		fix_3 = multFIX(fix_1,fix_2);
	}
}


float multFLOAT(float lng_1,float lng_2)
{
	return lng_1 * lng_2; 
}

void intFLOAT(void)
{
	float lng_1 = 23.45;
	float lng_2 = 1423.13;
	float lng_3 = 0;

	for(long a=0;a<100000000;a++)
	{
		lng_3 = multFLOAT(lng_1,lng_2);
	}

}






// The window procedure for the application's main window.
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    // Redraws the display when this app becomes active.
    case WM_ACTIVATEAPP:
      if (wParam) // is our app being activated?
        if (g_ddsw) // do we have a DDSurfaceWriter?
          DrawScreen(); // then redraw the screen!
      return 0;
    case WM_LBUTTONDOWN:
      // Closes the window when the user clicks the mouse.
		DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      // Main window destroyed -- message the app to exit.
      PostQuitMessage(0);
      return 0;
	case WM_KEYDOWN:
/*
	  if ((wParam =='8') || (wParam =='*')) CamX+=10;
	  if ((wParam =='i') || (wParam =='I')) CamX-=10;
	  if ((wParam =='9') || (wParam =='(')) CamY+=10;
	  if ((wParam =='o') || (wParam =='O')) CamY-=10;
	  if ((wParam =='0') || (wParam ==')')) CamZ+=10;
	  if ((wParam =='p') || (wParam =='P')) CamZ-=10;
	  if ((wParam =='g') || (wParam =='G')) an_X+=10;
	  if ((wParam =='b') || (wParam =='B')) an_X-=10;
	  if ((wParam =='h') || (wParam =='H')) an_Y+=10;
	  if ((wParam =='n') || (wParam =='N')) an_Y-=10;
	  if ((wParam =='j') || (wParam =='J')) an_Z+=10;
	  if ((wParam =='m') || (wParam =='M')) an_Z-=10;
*/
	  
	  if ((wParam =='h') || (wParam =='H')) an_Y+=10;
	  if ((wParam =='k') || (wParam =='K')) an_Y-=10;
	  if ((wParam =='u') || (wParam =='U')) 
	  {
		  CamZ += MyCos(an_Y)*20;
		  CamX -= MySin(an_Y)*20;
	  }

  	  if ((wParam =='m') || (wParam =='M')) 
	  {
		  CamZ -= MyCos(an_Y)*20;
		  CamX += MySin(an_Y)*20;
	  }


      if(an_X>359) an_X = an_X - 360;
      if(an_Y>359) an_Y = an_Y - 360;
      if(an_Z>359) an_Z = an_Z - 360;
      if(an_X<0) an_X = 360 + an_X;
      if(an_Y<0) an_Y = 360 + an_Y;
      if(an_Z<0) an_Z = 360 + an_Z;

	  if (((wParam =='q') || (wParam =='Q')) && (chk_TextureShow != -1))
		{
		  	ClearScreen();
			chk_TextureShow--;
		}
	  if (((wParam =='w') || (wParam =='W')) && (Texture[chk_TextureShow+1].bmWidth>0))
	  {
		  ClearScreen();
		  chk_TextureShow++;
	  }

	  
	  if ((wParam =='e') || (wParam =='E')) chk_Wire = !chk_Wire;
	  if ((wParam =='f') || (wParam =='F')) chk_FastMode = !chk_FastMode;
	  if ((wParam =='a') || (wParam =='A')) chk_Alias = !chk_Alias;
	  if ((wParam =='s') || (wParam =='S')) chk_Shadow = !chk_Shadow;
	  if ((wParam =='d') || (wParam =='D')) chk_Lighting = !chk_Lighting;
	  if ((wParam =='t') || (wParam =='T')) chk_Textures = !chk_Textures;
	  if (((wParam =='z') || (wParam =='Z')) && DefRenderStp>1) DefRenderStp--;
	  if (((wParam =='x') || (wParam =='X')) && DefRenderStp<11) 
	  {
		  ClearScreen();
		  DefRenderStp++;
	  }
	  if ((wParam =='1') || (wParam =='!')) LS[0].Enabled = !LS[0].Enabled;
	  if ((wParam =='2') || (wParam =='@')) LS[1].Enabled = !LS[1].Enabled;
	  if ((wParam =='3') || (wParam =='#')) LS[2].Enabled = !LS[2].Enabled;
	  if ((wParam =='4') || (wParam =='$')) LS[3].Enabled = !LS[3].Enabled;
	  if ((wParam =='5') || (wParam =='%')) LS[4].Enabled = !LS[4].Enabled;
	  if ((wParam =='6') || (wParam =='^')) LS[5].Enabled = !LS[5].Enabled;
	  if ((wParam =='7') || (wParam =='&')) LS[6].Enabled = !LS[6].Enabled;

	  if ((wParam =='o') || (wParam =='O'))
	  {
		  intFIX();
		  ErrorMessage("Done FIX", NULL);
	  }
	  if ((wParam =='p') || (wParam =='P'))
	  {
		  intFLOAT();
		  ErrorMessage("Done FLOAT", NULL);
	  }

	  return 0;

    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

inline void DrawScreen()
{
	
  RotateWorld();

  // Variable for error return values.
  HRESULT hr;

  // Lock the DDSurfaceWriter for writing.
  if (FAILED(hr = g_ddsw->Lock(NULL, DDLOCK_WAIT)))
  {
    ErrorMessage("Lock failed.", hr);
    return;
  }

  // Move back to the top of the screen.
  //g_ddsw->Move(0, 0);

  if(chk_Wire)
  {
	  RenderW();
  } else {
	  Render();
  }

  if(chk_TextureShow!=-1) DrawTexture();

  // Unlock the DDSurfaceWriter. (Failure to do so leaves the computer frozen!)
  if (FAILED(hr = g_ddsw->Unlock()))
		ErrorMessage("Unlock failed.", hr);

}

// Error message function.
void ErrorMessage(LPCSTR lpText, HRESULT hResult)
{
  LPSTR lpString = new CHAR[lstrlen(lpText) + 14];
                             
  wsprintf(lpString, "%s (0x%08x)", lpText, hResult);
  MessageBox(g_hwndMain, lpString, "Error", MB_ICONEXCLAMATION | MB_OK | MB_TOPMOST);
  delete [] lpString;
}

inline void DrawScreenB()
{
  // Wow the ligts go around and around.

  tmp+=3;
  if(tmp>359) tmp = tmp - 360;

  //MeshLights();

  LS[0].Lx=MyCos(tmp)*90;
  LS[0].Ly=MySin(tmp)*90;
  LS[1].Lx=MyCos((tmp+25))*80;
  LS[1].Ly=MySin((tmp+25))*80;
  LS[2].Lx=MyCos((tmp+50))*90;
  LS[2].Ly=MySin((tmp+50))*90;

  // and the box too, wooo.

  tmp2++;
  if(tmp2>359) tmp2 = tmp2 - 360;
  RotatePntExB(0, 0, tmp2);
  a = 3;CreateBox(a, -25, -25, (MyCos(tmp2) * 15)+35, 50, 50, 50);
  a=3;Transform(Ob[a].sX, Ob[a].sY, Ob[a].sZ); Transform(Ob[a].dAx, Ob[a].dAy, Ob[a].dAz); Transform(Ob[a].dBx, Ob[a].dBy, Ob[a].dBz);//CalcNorm(Ob[a]);
  a=4;Transform(Ob[a].sX, Ob[a].sY, Ob[a].sZ); Transform(Ob[a].dAx, Ob[a].dAy, Ob[a].dAz); Transform(Ob[a].dBx, Ob[a].dBy, Ob[a].dBz);//CalcNorm(Ob[a]);
  a=5;Transform(Ob[a].sX, Ob[a].sY, Ob[a].sZ); Transform(Ob[a].dAx, Ob[a].dAy, Ob[a].dAz); Transform(Ob[a].dBx, Ob[a].dBy, Ob[a].dBz);//CalcNorm(Ob[a]);
  a=6;Transform(Ob[a].sX, Ob[a].sY, Ob[a].sZ); Transform(Ob[a].dAx, Ob[a].dAy, Ob[a].dAz); Transform(Ob[a].dBx, Ob[a].dBy, Ob[a].dBz);//CalcNorm(Ob[a]);
  a=7;Transform(Ob[a].sX, Ob[a].sY, Ob[a].sZ); Transform(Ob[a].dAx, Ob[a].dAy, Ob[a].dAz); Transform(Ob[a].dBx, Ob[a].dBy, Ob[a].dBz);//CalcNorm(Ob[a]);
  a=8;Transform(Ob[a].sX, Ob[a].sY, Ob[a].sZ); Transform(Ob[a].dAx, Ob[a].dAy, Ob[a].dAz); Transform(Ob[a].dBx, Ob[a].dBy, Ob[a].dBz);//CalcNorm(Ob[a]);

	
  RotateWorld();

  // Variable for error return values.
  HRESULT hr;

  // Lock the DDSurfaceWriter for writing.
  if (FAILED(hr = g_ddsw->Lock(NULL, DDLOCK_WAIT)))
  {
    ErrorMessage("Lock failed.", hr);
    return;
  }

  // Move back to the top of the screen.
  //g_ddsw->Move(0, 0);

  Render();

  if(chk_TextureShow!=-1) DrawTexture();

  // Unlock the DDSurfaceWriter. (Failure to do so leaves the computer frozen!)
  if (FAILED(hr = g_ddsw->Unlock()))
		ErrorMessage("Unlock failed.", hr);

}


inline void ClearScreen()
{

	// Variable for error return values.
  HRESULT hr;

  // Lock the DDSurfaceWriter for writing.
  if (FAILED(hr = g_ddsw->Lock(NULL, DDLOCK_WAIT)))
  {
    ErrorMessage("Lock failed.", hr);
    return;
  }
  // Move back to thetop of the screen.
  
  g_ddsw->Move(0, 0);
  float Xr,Yr;

  // Main screen loop
  for(Yr = -100;Yr<101;Yr++)	// DefRenderStp
  {
	  for(Xr = -160;Xr<161;Xr++)
	  {
		  g_ddsw->DrawRGBPixel(RGB(0, 0, 0));
	  }
	  // Move cursor to start of next row
	  g_ddsw->NextRow();
  }


  // Unlock the DDSurfaceWriter. (Failure to do so leaves the computer frozen!)
  if (FAILED(hr = g_ddsw->Unlock()))
  {
    ErrorMessage("Unlock failed.", hr);
  }
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
inline float MyCos(int angle)
{
	if(angle>359) angle = angle - 360;
	if(angle<0) angle = angle + 360;
	return LocalCOS[angle];
}

inline float MySin(int angle)
{
	if(angle>359) angle = angle - 360;
	if(angle<0) angle = angle + 360;
	return LocalSIN[angle];
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rotate the world poly set and lights about CamX,CamY,CamZ
//	return new world as Obt & LSt
//	perform basic culling.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline RotateWorld()
{

	RotatePntExB(-an_X, an_Y, an_Z);
	long b=0,c=0;

	for(a=0;a<NOBS;a++)
	{
		Obt[b] = Ob[a];
		Obt[b].sX -= CamX ; Obt[b].sY -= CamY ; Obt[b].sZ -= CamZ;
		Transform(Obt[b].sX, Obt[b].sY, Obt[b].sZ);

		// Kill polys that are furher then 2000 units away.
		if((sqrt(Obt[b].sX * Obt[b].sX + Obt[b].sY * Obt[b].sY + Obt[b].sZ * Obt[b].sZ)) <2000)
		{
			Transform(Obt[b].dAx, Obt[b].dAy, Obt[b].dAz);
			Transform(Obt[b].dBx, Obt[b].dBy, Obt[b].dBz);
			CalcNorm(Obt[b]);
			if(FalcTest(Obt[b])==true)
			{
				// remove polygons that we cant see.
				if (0 >= CosAngle(Obt[b].sX+Obt[b].nX ,  Obt[b].sY+Obt[b].nY, Obt[b].sZ+Obt[b].nZ,  Obt[b].sX,  Obt[b].sY, Obt[b].sZ))
				{
					Obz[c] = Obt[b];
					c++;
				} else {
					if(chk_Shadow==false) continue;
				}
			}


				// Past the tests? then include shadowing polygon.
			b++;
		}
	}
	NOBSt = b;
	NOBSz = c;

	b = 0;
	for(a=0;a<LSN;a++)
	{
		if(LS[a].Enabled)
		{
			LSt[b] = LS[a];
			//LSt[b].LastPolyHit = rand()%NOBSt;
			LSt[b].Lx -= CamX ; LSt[b].Ly -= CamY ; LSt[b].Lz -= CamZ;
			Transform(LSt[b].Lx, LSt[b].Ly, LSt[b].Lz);

			if((sqrt(LSt[b].Lx * LSt[b].Lx + LSt[b].Ly * LSt[b].Ly + LSt[b].Lz * LSt[b].Lz)) <2000)
			{
				if(LSt[b].Type == 2)
				{
					LSt[b].Dx -= CamX ; LSt[b].Dy -= CamY ; LSt[b].Dz -= CamZ;
					Transform(LSt[b].Dx, LSt[b].Dy, LSt[b].Dz);
				}
				b++;
			}
		}
	}
	LSNt = b;
}


inline bool FalcTest(Poly &Mesh)
{
	float sAz = Mesh.sZ + Mesh.dAz;
	float sAy = Mesh.sY + Mesh.dAy;
	float sAx = Mesh.sX + Mesh.dAx;
	float sBz = Mesh.sZ + Mesh.dBz;
	float sBy = Mesh.sY + Mesh.dBy;
	float sBx = Mesh.sX + Mesh.dBx;


	if((Mesh.sZ>TMS) || (sAz > TMS) || (sBz > TMS) || (sAz + sBz > TMS))
	{
		//float wslope = -(160 / TMS);
		//float hslope = -(100 / TMS);
		//if((Mesh.sZ * 1000>Mesh.sX) || (sAz * 1000>sAx) || (sBz * 1000>sBx))
		//if((Mesh.sZ * -1000<Mesh.sX) || (sAz * -1000<sAx) || (sBz * -1000<sBx))
		//if((Mesh.sZ * 1000>Mesh.sY) || (sAz * 1000>sAy) || (sBz * 1000>sBy))
		//if((Mesh.sZ * -1000<Mesh.sY) || (sAz * -1000<sAy) || (sBz * -1000<sBy))
			return true;
		
	}
	return false;
}



inline void MeshLights()
{
	a = 0; LS[a].Type = 2;
		   LS[a].Enabled=true;
		   LS[a].Lx = -0;  LS[a].Ly = -80;  LS[a].Lz = 0; LS[a].Colour_R = 1; LS[a].Colour_G = 0.4; LS[a].Colour_B = 0.4;
		   LS[a].Dx = -400; LS[a].Dy = 50;  LS[a].Dz = 400; LS[a].Cone = 0.3;
		   LS[a].FuzFactor = 0.6;
	a = 1; LS[a].Type = 2;
		   LS[a].Enabled=true;
		   LS[a].Lx = -0;  LS[a].Ly = -80; LS[a].Lz = 0; LS[a].Colour_R = 0.4; LS[a].Colour_G = 1; LS[a].Colour_B = 0.4;
		   LS[a].Dx = 0; LS[a].Dy = 50;  LS[a].Dz = 400; LS[a].Cone = 0.3;
		   LS[a].FuzFactor = 0.6;
	a = 2; LS[a].Type = 2;
		   LS[a].Enabled=true;	   
		   LS[a].Lx = -0; LS[a].Ly = -80;  LS[a].Lz = 0; LS[a].Colour_R = 0.4; LS[a].Colour_G = 0.4; LS[a].Colour_B = 1;
		   LS[a].Dx = 400; LS[a].Dy = 50;  LS[a].Dz = 400; LS[a].Cone = 0.3;
		   LS[a].FuzFactor = 0.6;

	a = 3; LS[a].Type = 1;
		   LS[a].Enabled=true;
		   LS[a].Lx = -380; LS[a].Ly = -50;  LS[a].Lz = 380; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	a = 4; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = 380; LS[a].Ly = -50;  LS[a].Lz = 280; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	a = 5; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = -380; LS[a].Ly = -50;  LS[a].Lz = -380; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	a = 6; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = 380; LS[a].Ly = -50;  LS[a].Lz = -380; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;

	a = 6; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = 380; LS[a].Ly = -50;  LS[a].Lz = -380; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	   
	a = 7; LS[a].Type = 2;
		   LS[a].Enabled=true;	   
		   LS[a].Lx = 0; LS[a].Ly = -90;  LS[a].Lz = -900; LS[a].Colour_R = 1; LS[a].Colour_G = 0.4; LS[a].Colour_B = 0.4;
		   LS[a].Dx = 0; LS[a].Dy = 90;  LS[a].Dz = -1100; LS[a].Cone = 0.5;
		   LS[a].FuzFactor = 0.6;
		   

   	a = 8; LS[a].Type = 1;
		   LS[a].Enabled=true;
		   LS[a].Lx = 500; LS[a].Ly = -50;  LS[a].Lz = -1300; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;

		   
   LSN = a+1;

	

// Done.

	//walls

	

	a = 0; CreatePlaneEx(Ob[a], 400, -100, -400, 0, 0, 800, -800, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 0;Ob[a].SurfaceMultW = 6;Ob[a].SurfaceMultH = 6;

	a = 1; CreatePlaneEx(Ob[a], -400, 100, -400, 0, 0, 800, 800, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 2;Ob[a].SurfaceMultW = 2;Ob[a].SurfaceMultH = 2;

	a = 2; CreatePlaneEx(Ob[a], -400, -100, -400, 0, 0, 800, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a = 3; CreatePlaneEx(Ob[a], 400, -100, 400, 0, 0, -800, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a = 4; CreatePlaneEx(Ob[a], -400, -100, 400, 800, 0, 0, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a = 5; CreatePlaneEx(Ob[a], 400, -100, -400, -300, 0, 0, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a = 6; CreatePlaneEx(Ob[a], -100, -100, -400, -300, 0, 0, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a = 7; CreatePlaneEx(Ob[a], 398, -50, 250, 0, 0, -200, 0, 100, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 4;Ob[a].SurfaceMultW = 1;Ob[a].SurfaceMultH = 1;


	a++;CreateBox(a, -400, 40, 100, 300, 10, 250);
	a++;CreateBox(a, -110, 50, 100, 10, 50, 10);

	a++;CreateBox(a, 300, -80, 300, 100, 180, 100);
	Ob[a-4].SurfaceTexture = -1;Ob[a-4].Surface_B = 0.2 ; Ob[a-4].Surface_G = 0.2 ; Ob[a-4].Surface_R = 0.2;
	Ob[a-3].SurfaceTexture = -1;Ob[a-3].Surface_B = 0.2 ; Ob[a-3].Surface_G = 0.2 ; Ob[a-3].Surface_R = 0.2;
	Ob[a-2].SurfaceTexture = -1;Ob[a-2].Surface_B = 0.2 ; Ob[a-2].Surface_G = 0.2 ; Ob[a-2].Surface_R = 0.2;
	Ob[a-1].SurfaceTexture = -1;Ob[a-1].Surface_B = 0.2 ; Ob[a-1].Surface_G = 0.2 ; Ob[a-1].Surface_R = 0.2;
	Ob[a].SurfaceTexture = -1;  Ob[a].Surface_B = 0.2 ; Ob[a].Surface_G = 0.2 ; Ob[a].Surface_R = 0.2;

	Ob[a-5].SurfaceTexture = 5; Ob[a-5].SurfaceMultW = 1;Ob[a-5].SurfaceMultH = 1;
	Ob[a-5].Surface_B = 5 ; Ob[a-5].Surface_G = 5 ; Ob[a-5].Surface_R = 5;

	a++; CreatePlaneEx(Ob[a], 100, -100, -1000, 0, 0, 600, -200, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 0;Ob[a].SurfaceMultW = 6;Ob[a].SurfaceMultH = 6;

	a++; CreatePlaneEx(Ob[a], -100, 100, -1000, 0, 0, 600, 200, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 2;Ob[a].SurfaceMultW = 2;Ob[a].SurfaceMultH = 2;

	a++; CreatePlaneEx(Ob[a], -100, -100, -1000, 0, 0, 600, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a++; CreatePlaneEx(Ob[a], 100, -100, -400, 0, 0, -600, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a++; CreatePlaneEx(Ob[a], 400, -100, -1200, -800, 0, 0, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;


	
	a++; CreatePlaneEx(Ob[a], -400, 100, -1200, 0, 0, 200, 1000, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 0;Ob[a].SurfaceMultW = 2;Ob[a].SurfaceMultH = 6;
	a++; CreatePlaneEx(Ob[a], -400, -100, -1000, 0, 0, -200, 1000, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 0;Ob[a].SurfaceMultW = 2;Ob[a].SurfaceMultH = 6;

	a++; CreatePlaneEx(Ob[a], -400, -100, -1000, 300, 0, 0, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;
	a++; CreatePlaneEx(Ob[a], 100, -100, -1000, 500, 0, 0, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;

	a++; CreatePlaneEx(Ob[a], -400, -100, -1200, 0, 0, 200, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 4;Ob[a].SurfaceMultH = 1;



	a++; CreatePlaneEx(Ob[a], 600, -100, -2800, 0, 0, 1600, -200, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 0;Ob[a].SurfaceMultW = 6;Ob[a].SurfaceMultH = 6;

	a++; CreatePlaneEx(Ob[a], 400, 100, -2800, 0, 0, 1600, 200, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 2;Ob[a].SurfaceMultW = 6;Ob[a].SurfaceMultH = 2;

	a++; CreatePlaneEx(Ob[a], 400, -100, -2800, 0, 0, 1600, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 6;Ob[a].SurfaceMultH = 1;

	a++; CreatePlaneEx(Ob[a], 600, -100, -1000, 0, 0, -1800, 0, 200, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;Ob[a].SurfaceMultW = 6;Ob[a].SurfaceMultH = 1;


	NOBS = a+1;
}


inline void MeshLightsB()
{
	a = 0; LS[a].Type = 2;
		   LS[a].Enabled=true;
		   LS[a].Lx = 15;  LS[a].Ly = -15;  LS[a].Lz = 16; LS[a].Colour_R = 1; LS[a].Colour_G = 0.4; LS[a].Colour_B = 0.4;
		   LS[a].Dx = -15; LS[a].Dy = -15;  LS[a].Dz = 20; LS[a].Cone = 0.3;
		   LS[a].FuzFactor = 0.6;
	a = 1; LS[a].Type = 2;
		   LS[a].Enabled=true;
		   LS[a].Lx = 15;  LS[a].Ly = -15; LS[a].Lz = 16; LS[a].Colour_R = 0.4; LS[a].Colour_G = 1; LS[a].Colour_B = 0.4;
		   LS[a].Dx = 15; LS[a].Dy = -15;  LS[a].Dz = 20; LS[a].Cone = 0.3;
		   LS[a].FuzFactor = 0.6;
	a = 2; LS[a].Type = 2;
		   LS[a].Enabled=true;	   
		   LS[a].Lx = -15; LS[a].Ly = 15;  LS[a].Lz = 16; LS[a].Colour_R = 0.4; LS[a].Colour_G = 0.4; LS[a].Colour_B = 1;
		   LS[a].Dx = -15; LS[a].Dy = 15;  LS[a].Dz = 20; LS[a].Cone = 0.3;
		   LS[a].FuzFactor = 0.6;

	a = 3; LS[a].Type = 1;
		   LS[a].Enabled=true;
		   LS[a].Lx = -15; LS[a].Ly = -5;  LS[a].Lz = 3; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	a = 4; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = 1; LS[a].Ly = -15;  LS[a].Lz = 3; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	a = 5; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = 15; LS[a].Ly = 1;  LS[a].Lz = 3; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	a = 6; LS[a].Type = 1;
		   LS[a].Enabled=false;
		   LS[a].Lx = 1; LS[a].Ly = 15;  LS[a].Lz = 3; LS[a].Colour_R = 0.5; LS[a].Colour_G = 0.5; LS[a].Colour_B = 0.5;
	LSN = a+1;

	

// Done.


	//walls
	a = 0; CreatePlaneEx(Ob[a], -20, -20, 20, 40, 0, 0, 0, 40, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 2;
	Ob[a].SurfaceMultW = 2;
	Ob[a].SurfaceMultH = 2;
	a = 1; CreatePlaneEx(Ob[a], -20, -20, 0, 0, 0, 20, 0, 40, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 2;
	Ob[a].SurfaceMultH = 2;
	a = 2; CreatePlaneEx(Ob[a], 20, -20, 0, 0, 0, 20, -40, 0, 0);  Ob[a].Surface_R = 1; Ob[a].Surface_G = 1; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 2;
	Ob[a].SurfaceMultH = 2;
	a = 3;		CreateBox(a, -5, -5, 10, 10, 10, 10);

  	Ob[3].SurfaceTexture = 0;
	Ob[4].SurfaceTexture = 0;
	Ob[5].SurfaceTexture = 0;
	Ob[6].SurfaceTexture = 0;
	Ob[7].SurfaceTexture = 0;
	Ob[8].SurfaceTexture = 0;

	

	a++; CreatePlaneEx(Ob[a], -20, 15, 12, 10, 0, 0, 0, 3, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -20, 12, 10, 10, 0, 0, 0, 3, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -20, 9, 8, 10, 0, 0, 0, 3, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -20, 6, 6, 10, 0, 0, 0, 3, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -20, 3, 4, 10, 0, 0, 0, 3, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -20, 0, 2, 10, 0, 0, 0, 3, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -20, -20, 0, 10, 0, 0, 0, 18, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;
	a++; CreatePlaneEx(Ob[a], -10, -20, 0, 30, 0, 0, 0, 8, 0); Ob[a].Surface_R = 1; Ob[a].Surface_G = 1 ; Ob[a].Surface_B = 1;
	Ob[a].SurfaceTexture = 1;


	a++;		CreateBox(a, -5, -5, -50, 10, 10, 10);

	NOBS = a+1;

	// Scale
	float Fact = 0.5;

	for(a=0;a<LSN;a++)
	{
		LS[a].Colour_R = LS[a].Colour_R * Fact;
		LS[a].Colour_G = LS[a].Colour_G * Fact;
		LS[a].Colour_B = LS[a].Colour_B * Fact;
	}
	
	Fact = float(0.2);
	for(a=0;a<LSN;a++)
	{
		LS[a].Lx = LS[a].Lx / Fact;
		LS[a].Ly = LS[a].Ly / Fact;
		LS[a].Lz = LS[a].Lz / Fact;
		LS[a].Dx = LS[a].Dx / Fact;
		LS[a].Dy = LS[a].Dy / Fact;
		LS[a].Dz = LS[a].Dz / Fact;

	}	

	for(a=0;a<NOBS;a++)
	{
		Ob[a].sX = Ob[a].sX / Fact;
		Ob[a].sY = Ob[a].sY / Fact;
		Ob[a].sZ = Ob[a].sZ / Fact;
		Ob[a].dAx = Ob[a].dAx / Fact;
		Ob[a].dAy = Ob[a].dAy / Fact;
		Ob[a].dAz = Ob[a].dAz / Fact;
		Ob[a].dBx = Ob[a].dBx / Fact;
		Ob[a].dBy = Ob[a].dBy / Fact;
		Ob[a].dBz = Ob[a].dBz / Fact;
	}


}



inline void SetUpWorld()
{
	// Calculate Sin/Cos Tables
	for(tmp=0;tmp<360;tmp++)
	{
		LocalSIN[tmp]=float(sin(tmp*Rad));
		LocalCOS[tmp]=float(cos(tmp*Rad));
	}
	
	MeshLights();
	
    LoadTexture(Texture[0],"D:\\Dave Lib\\3D\\Media\\textures\\ground\\rocksideoneshort.bmp"); // roof
	LoadTexture(Texture[1],"D:\\Dave Lib\\3D\\Media\\textures\\wall\\aardfflrbr256_1.bmp"); // walls
	LoadTexture(Texture[2],"D:\\Dave Lib\\3D\\Media\\textures\\ground\\bluesidewall2.bmp"); // ground
	LoadTexture(Texture[3],"D:\\Dave Lib\\3D\\Media\\textures\\wall\\wood.bmp"); // bench
	LoadTexture(Texture[4],"D:\\Dave Lib\\3D\\Media\\textures\\sky\\sky1_farmland-c.bmp"); // bench
	SKYV = 155;
	LoadTexture(Texture[5],"D:\\Dave Lib\\3D\\Media\\textures\\things\\objects06.bmp"); // coke

	
	//ADD ME
    //CamX = 96; CamY = 200; CamZ = 67;
	//an_X = 34; an_Y = -20;	an_Z = -41;

	//an_X = 0; an_Y = 0; an_Z = 0;
	CamX = 0; CamY = 0; CamZ = -300;

	//If an_X > 360 Then an_X = 0
	//If an_Y > 360 Then an_Y = 0
	//If an_Z > 360 Then an_Z = 0

	TMS = -300;//TMS = -300;
	chk_Alias = false;
	chk_Shadow = true;
	chk_Textures = true;
	DefRenderStp = 3;
	chk_TextureShow = -1;
	chk_Lighting = true;

	BGC_R = 80;
	BGC_G = 80;
	BGC_B = 80;

}



inline void SetUpWorldB()
{
	// Calculate Sin/Cos Tables
	for(tmp=0;tmp<360;tmp++)
	{
		LocalSIN[tmp]=float(sin(tmp*Rad));
		LocalCOS[tmp]=float(cos(tmp*Rad));
	}

	MeshLightsB();
	

    LoadTexture(Texture[4],"c:\\temp\\s_morest.bmp");
    LoadTexture(Texture[6],"c:\\temp\\tt.bmp");
    LoadTexture(Texture[5],"c:\\temp\\t24z.bmp");
    LoadTexture(Texture[2],"c:\\temp\\wood.bmp");
    LoadTexture(Texture[3],"c:\\temp\\t.bmp");
    LoadTexture(Texture[0],"c:\\temp\\t24b.bmp");
    LoadTexture(Texture[1],"c:\\temp\\t24m.bmp");

	//ADD ME
    //CamX = 96; CamY = 200; CamZ = 67;
	//an_X = 34; an_Y = -20;	an_Z = -41;

	//an_X = 0; an_Y = 0; an_Z = 0;
	CamX = 0; CamY = 0; CamZ = -300;

	//If an_X > 360 Then an_X = 0
	//If an_Y > 360 Then an_Y = 0
	//If an_Z > 360 Then an_Z = 0

	TMS = -300;//TMS = -300;
	chk_Alias = false;
	chk_Shadow = true;
	chk_Textures = true;
	DefRenderStp = 3;
	chk_TextureShow = -1;

	BGC_R = 50;
	BGC_G = 50;
	BGC_B = 50;

}


inline float CosAngle(float AX, float AY, float AZ, float bx, float by, float bz)
{
	return float((AX * bx + AY * by + AZ * bz) / (sqrt(AX * AX + AY * AY + AZ * AZ) * sqrt(bx * bx + by * by + bz * bz)));
}



inline void CreatePlaneEx(Poly &Mesh, float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float Z3)
{
    Mesh.pType = 1;
    Mesh.sX = X1; Mesh.sY = Y1; Mesh.sZ = Z1;
    Mesh.dAx = X2; Mesh.dAy = Y2; Mesh.dAz = Z2;
    Mesh.dBx = X3; Mesh.dBy = Y3; Mesh.dBz = Z3;
	Mesh.SurfaceTexture = -1;
	Mesh.SurfaceMultH = 1;
	Mesh.SurfaceMultW = 1;
	CalcNorm(Mesh);
}

inline void CalcNorm(Poly &Mesh)
{
	// Calculate normal
	Mesh.nX = -float(Mesh.dAy * Mesh.dBz - Mesh.dAz * Mesh.dBy);
	Mesh.nY = -float(Mesh.dAz * Mesh.dBx - Mesh.dAx * Mesh.dBz);
	Mesh.nZ = -float(Mesh.dAx * Mesh.dBy - Mesh.dAy * Mesh.dBx);
}


inline void CreateBox(long &StartInd, float X1, float Y1, float Z1, float wX1, float wY1, float wZ1)
{
    CreatePlaneEx(Ob[StartInd], X1 + wX1, Y1 + wY1, Z1, -wX1, 0, 0, 0, -wY1, 0);
	Ob[StartInd].Surface_R = 1;Ob[StartInd].Surface_G = 1;Ob[StartInd].Surface_B = 1;
	Ob[StartInd].SurfaceTexture = 3;Ob[StartInd].SurfaceMultH = 2;Ob[StartInd].SurfaceMultW=2;
    StartInd+=1;
    CreatePlaneEx(Ob[StartInd], X1 + wX1, Y1, Z1 + wZ1, -wX1, 0, 0, 0, wY1, 0);
	Ob[StartInd].Surface_R = 1; Ob[StartInd].Surface_G = 1; Ob[StartInd].Surface_B = 1;
	Ob[StartInd].SurfaceTexture = 3;Ob[StartInd].SurfaceMultH = 2;Ob[StartInd].SurfaceMultW=2;
    StartInd+=1;
    CreatePlaneEx(Ob[StartInd], X1 + wX1, Y1, Z1, -wX1, 0, 0, 0, 0, wZ1);
	Ob[StartInd].Surface_R = 1; Ob[StartInd].Surface_G = 1; Ob[StartInd].Surface_B = 1;
	Ob[StartInd].SurfaceTexture = 3;Ob[StartInd].SurfaceMultH = 2;Ob[StartInd].SurfaceMultW=2;
    StartInd+=1;
    CreatePlaneEx(Ob[StartInd], X1, wY1 + Y1, Z1, wX1, 0, 0, 0, 0, wZ1);
	Ob[StartInd].Surface_R = 1; Ob[StartInd].Surface_G = 1; Ob[StartInd].Surface_B = 1;
	Ob[StartInd].SurfaceTexture = 3;Ob[StartInd].SurfaceMultH = 2;Ob[StartInd].SurfaceMultW=2;
    StartInd+=1;
    CreatePlaneEx(Ob[StartInd], X1, Y1, Z1, 0, wY1, 0, 0, 0, wZ1);
	Ob[StartInd].Surface_R = 1; Ob[StartInd].Surface_G = 1; Ob[StartInd].Surface_B = 1;
	Ob[StartInd].SurfaceTexture = 3;Ob[StartInd].SurfaceMultH = 2;Ob[StartInd].SurfaceMultW=2;
    StartInd+=1;
    CreatePlaneEx(Ob[StartInd], X1 + wX1, Y1 + wY1, Z1, 0, -wY1, 0, 0, 0, wZ1);
	Ob[StartInd].Surface_R = 1; Ob[StartInd].Surface_G = 1; Ob[StartInd].Surface_B = 1;
	Ob[StartInd].SurfaceTexture = 3;Ob[StartInd].SurfaceMultH = 2;Ob[StartInd].SurfaceMultW=2;
}



inline float InterPlane(float a11, float a21, float a31, float a12, float a22, float a32, float a13, float a23, float a33, float b1, float b2, float b3)
{
	float D = a11 * a22 * a33 + a12 * a23 * a31 + a13 * a21 * a32 - a13 * a22 * a31 - a11 * a23 * a32 - a33 * a12 * a21;
	if (D > 0) {
		float D2 = a11 * b2 * a33 + b1 * a23 * a31 + a13 * a21 * b3 - a13 * b2 * a31 - a23 * b3 * a11 - a33 * b1 * a21;
		float D3 = a11 * a22 * b3 + a12 * b2 * a31 + b1 * a21 * a32 - b1 * a22 * a31 - b2 * a32 * a11 - b3 * a12 * a21;
		tiA = D2 / D, tiB = D3 / D;
		if ((tiA >= 0) && (tiA <= 1) && (tiB >= 0) && (tiB <= 1)) {
			float D1 = b1 * a22 * a33 + a12 * a23 * b3 + a13 * b2 * a32 - a13 * a22 * b3 - a23 * a32 * b1 - a33 * a12 * b2;
			float L = D1 / D;
			iX = oX + L * a11;
			iY = oY + L * a21;
			iZ = oZ + L * a31;
			return -D;
		}
	}
	iX = 0;	iY = 0;	iZ = 0;
	return 0;
}


inline float InterPlaneEx(float a11, float a21, float a31, float a12, float a22, float a32, float a13, float a23, float a33, float b1, float b2, float b3)
{
	float D = a11 * a22 * a33 + a12 * a23 * a31 + a13 * a21 * a32 - a13 * a22 * a31 - a11 * a23 * a32 - a33 * a12 * a21;

	if (D > 0) {
		float D2 = a11 * b2 * a33 + b1 * a23 * a31 + a13 * a21 * b3 - a13 * b2 * a31 - a23 * b3 * a11 - a33 * b1 * a21;
		float D3 = a11 * a22 * b3 + a12 * b2 * a31 + b1 * a21 * a32 - b1 * a22 * a31 - b2 * a32 * a11 - b3 * a12 * a21;
		float tiA = D2 / D, tiB = D3 / D;
		if ((tiA >= 0) && (tiA <= 1) && (tiB >= 0) && (tiB <= 1)) {
			float D1 = b1 * a22 * a33 + a12 * a23 * b3 + a13 * b2 * a32 - a13 * a22 * b3 - a23 * a32 * b1 - a33 * a12 * b2;
			float L = D1 / D;
			if ((L < 0) && (L >= -1)) return D;
		}
	}
	return 0;
}


inline void MatMult(float mat1[4][4], float mat2[4][4])
{
    float temp[4][4];
    for(short unsigned int i = 0; i<4; i++)
        for(short unsigned int j = 0; j<4; j++)
            temp[i][j] = (mat1[i][0] * mat2[0][j]) + (mat1[i][1] * mat2[1][j]) + (mat1[i][2] * mat2[2][j]) + (mat1[i][3] * mat2[3][j]);

    for(i= 0;i<4;i++)
	{
        mat1[i][0] = temp[i][0];
        mat1[i][1] = temp[i][1];
        mat1[i][2] = temp[i][2];
        mat1[i][3] = temp[i][3];
    }
}

inline void RotatePntExB(int AngX, int AngY, int AngZ)
{   
    float rmat[4][4];

    Matrix[0][0] = 1; Matrix[0][1] = 0; Matrix[0][2] = 0; Matrix[0][3] = 0;
    Matrix[1][0] = 0; Matrix[1][1] = 1; Matrix[1][2] = 0; Matrix[1][3] = 0;
    Matrix[2][0] = 0; Matrix[2][1] = 0; Matrix[2][2] = 1; Matrix[2][3] = 0;
    Matrix[3][0] = 0; Matrix[3][1] = 0; Matrix[3][2] = 0; Matrix[3][3] = 1;
    // Z
    rmat[0][0] = MyCos(AngZ)	; rmat[0][1] = MySin(AngZ)	; rmat[0][2] = 0; rmat[0][3] = 0;
    rmat[1][0] = -MySin(AngZ)	; rmat[1][1] = MyCos(AngZ)	; rmat[1][2] = 0; rmat[1][3] = 0;
    rmat[2][0] = 0				; rmat[2][1] = 0			; rmat[2][2] = 1; rmat[2][3] = 0;
    rmat[3][0] = 0				; rmat[3][1] = 0			; rmat[3][2] = 0; rmat[3][3] = 1;
    MatMult(Matrix, rmat);
    // X
    rmat[0][0] = 1;               rmat[0][1] = 0			;rmat[0][2] = 0;               rmat[0][3] = 0;
    rmat[1][0] = 0;               rmat[1][1] = MyCos(AngX)	;rmat[1][2] = MySin(AngX); rmat[1][3] = 0;
    rmat[2][0] = 0;               rmat[2][1] = -MySin(AngX)	;rmat[2][2] = MyCos(AngX); rmat[2][3] = 0;
    rmat[3][0] = 0;               rmat[3][1] = 0			;rmat[3][2] = 0;               rmat[3][3] = 1;
    MatMult(Matrix, rmat);
    // Y
    rmat[0][0] = MyCos(AngY); rmat[0][1] = 0; rmat[0][2] = -MySin(AngY)	; rmat[0][3] = 0;
    rmat[1][0] = 0				; rmat[1][1] = 1; rmat[1][2] = 0				; rmat[1][3] = 0;
    rmat[2][0] = MySin(AngY); rmat[2][1] = 0; rmat[2][2] = MyCos(AngY)	; rmat[2][3] = 0;
    rmat[3][0] = 0				; rmat[3][1] = 0; rmat[3][2] = 0				; rmat[3][3] = 1;

    MatMult(Matrix, rmat);
}

inline void Translate(float Xt, float Yt, float Zt)
{   
    float tmat[4][4];

    tmat[0][0] = 1	; tmat[0][1] = 0 ; tmat[0][2] = 0; tmat[0][3] = 0;
    tmat[1][0] = 0	; tmat[1][1] = 1 ; tmat[1][2] = 0; tmat[1][3] = 0;
    tmat[2][0] = 0	; tmat[2][1] = 0 ; tmat[2][2] = 1; tmat[2][3] = 0;
    tmat[3][0] = Xt	; tmat[3][1] = Yt; tmat[3][2] = Zt; tmat[3][3] = 1;
    MatMult(Matrix, tmat);
}

inline void Transform(float &pX, float &pY, float &pZ)
{
    float outX = pX * Matrix[0][0] + pY * Matrix[1][0] + pZ * Matrix[2][0] + Matrix[3][0];
    float outY = pX * Matrix[0][1] + pY * Matrix[1][1] + pZ * Matrix[2][1] + Matrix[3][1];
    float outZ = pX * Matrix[0][2] + pY * Matrix[1][2] + pZ * Matrix[2][2] + Matrix[3][2];
    pX = outX; pY = outY; pZ = outZ;
}



inline void Render()
{
	float SupSampSize = (0.33333 * DefRenderStp);

	float ccR,ccG,ccB;		// Temp Color

	// Define out fixed FP in space
	float rX = 0; float rY = 0; float rZ = 0;// TMS;
	oZ = 0 - TMS;
	// Main screen loop
	g_ddsw->Move(int(160 - ((320 / DefRenderStp)/2)), int(100 - ((200 / DefRenderStp)/2)));

	for(float Yr = -100;Yr<101;Yr += DefRenderStp)	// DefRenderStp
	{
		for(float Xr = -160;Xr<161;Xr += DefRenderStp)
		{
			// This is the point we cast the ray through
			oX = Xr; oY = Yr;

			// Reset our Point colour..
			ccR = 0;ccG = 0;ccB = 0;
			
			if(chk_FastMode)
			{
				TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
				if(chk_Alias)
				{
					// Sample around center point
					oX = Xr+SupSampSize; oY = Yr+SupSampSize;
					TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					oX = Xr-SupSampSize; oY = Yr-SupSampSize;
					TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					oX = Xr+SupSampSize; oY = Yr-SupSampSize;
					TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					oX = Xr-SupSampSize; oY = Yr+SupSampSize;
					TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					ccR = ccR / 5;ccG = ccG / 5;ccB = ccB / 5;
				}

			} else {
				TraceA(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
				if(chk_Alias)
				{
					// Sample around center point
					oX = Xr+SupSampSize; oY = Yr+SupSampSize;
					TraceA(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					oX = Xr-SupSampSize; oY = Yr-SupSampSize;
					TraceA(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					oX = Xr+SupSampSize; oY = Yr-SupSampSize;
					TraceA(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					oX = Xr-SupSampSize; oY = Yr+SupSampSize;
					TraceA(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
					ccR = ccR / 5;ccG = ccG / 5;ccB = ccB / 5;
				}
			}

			if(ccR > 255) ccR = 255;
			if(ccG > 255) ccG = 255;
			if(ccB > 255) ccB = 255;
			if(ccR < 0) ccR = 0;
			if(ccG < 0) ccG = 0;
			if(ccB < 0) ccB = 0;

			g_ddsw->DrawRGBPixel(RGB(ccR, ccG, ccB));
			//g_ddsw->DrawRawPixel(g_ddsw->RGBToRaw(RGB(ccR, ccG, ccB)));



			//////////////////////////////////////////////////////
			//////////////////////////////////////////////////////
		}
		// Move cursor to start of next row
		g_ddsw->NextRow();
		g_ddsw->MoveRight(int(160 - ((320 / DefRenderStp)/2)));		
	}

	g_ddsw->Move(0,0);
	if(chk_FastMode)
	{
		g_ddsw->DrawRGBPixel(RGB(255, 0, 0));
	} else {
		g_ddsw->DrawRGBPixel(RGB(0, 255, 0));
	}
}



void DrawLine(float X1,float Y1,float X2,float Y2)
{

	float LnX;
	int LnDir=1;


//	if(X2<X1)
	//{
	//	LnDir = -1;
	//	LnX = X2;
	//	X2 = X1;
	//	X1 = LnX;
	//}
	//if(Y2<Y1)
	//{
		//LnX = Y2;
		//Y2 = Y1;
		//Y1 = LnX;
	//}


	float Slope;
	if((X2 - X1)==0)
	{
		Slope = 0;
	} else {
		Slope = (Y2 - Y1) / (X2 - X1);
	}

	if(X1<0) X1 = 0;
	if(X1>320) X1 = 320;
	if(X2<0) X2 = 0;
	if(X2>320) X2 = 320;

	float Ycalc;
	for(LnX=0;LnX<(X2-X1);LnX++)
	{
		Ycalc = Y1+(LnX * Slope);
		if((Ycalc>0) && (Ycalc<200))
		{
			g_ddsw->Move(int(X1 + LnX),int(Ycalc));
			g_ddsw->DrawRGBPixel(RGB(255, 0, 0));
		}
	}
}



inline void RenderTF()
{
	float ccR,ccG,ccB;		// Temp Color
	// Define out fixed FP in space
	float rX = 0; float rY = 0; float rZ = TMS;

	// Main screen loop
	//g_ddsw->Move(int(160 - ((320 / DefRenderStp)/2)), int(100 - ((200 / DefRenderStp)/2)));

	tmp++;
	// This is the point we cast the ray through
	//oX = Xr; oY = Yr; oZ = 0 - TMS;
	// Reset our Point colour..
	ccR = 0;ccG = 0;ccB = 0;
	//TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
			
	float Div=30,Tms=10;

	// Create pointer to our transformed World sturcture.
	Poly &MyOb = Obz[NOBSz];
	
	long OBJ = -1;
	float iX2,iY2;
	float U,V;

	for(a = 0; a<NOBSz;a++)
	//for(a = 0; a<3;a++)
	{
		// Update pointer
		MyOb = Obz[a];	
	if(MyOb.SurfaceTexture!=-1)
	{

		float Nx = MyOb.dBx * 300;
		float Ny = MyOb.dBy * 300;
		float Nz = MyOb.dBz;
		float Mx = MyOb.dAx * 300;
		float My = MyOb.dAy * 300;
		float Mz = MyOb.dAz;
		float Px = MyOb.sX * 300;
		float Py = MyOb.sY * 300;
		float Pz = MyOb.sZ;


		float Oa = Nx * Py - Ny * Px;
		float Ha = Ny * Pz - Nz * Py;
		float Va = Nz * Px - Nx * Pz;
		float Ob = Mx * Py - My * Px;
		float Hb = My * Pz - Mz * Py;
		float Vb = Mz * Px - Mx * Pz;
		float Oc = My * Nx - Mx * Ny;
		float Hc = Mz * Ny - My * Nz;
		float Vc = Mx * Nz - Mz * Nx;

		
		//int start_x = -100;
		for(int j = -100; j<101;j+=DefRenderStp)
		{
			for(int i = -160; i<161;i+=DefRenderStp)
			{
			float a = Oa + i * Ha + j * Va;
			float b = Ob + i * Hb + j * Vb;
			float c = Oc + i * Hc + j * Vc;
				if(c < 0)
				{
					U = a / c;
					V = b / c;
        
					if((U >= 0) && (U <= 1) && (V >= 0) && (V <= 1))
					{
						RGBQUAD pXColor = GetPixel(Texture[MyOb.SurfaceTexture],long((U*MyOb.SurfaceMultW) * Texture[MyOb.SurfaceTexture].bmWidth), long((V*MyOb.SurfaceMultH) * Texture[MyOb.SurfaceTexture].bmHeight));
						ccR = BGC_R * ((float(pXColor.rgbRed) /255) * MyOb.Surface_R);
						ccG = BGC_G * ((float(pXColor.rgbGreen) /255) * MyOb.Surface_G);
						ccB = BGC_B * ((float(pXColor.rgbBlue) /255) * MyOb.Surface_R);

						g_ddsw->Move(i/DefRenderStp + 160,j/DefRenderStp + 100);
						ccR=ccR*2.5;
						ccG=ccG*2.5;
						ccB=ccB*2.5;
						g_ddsw->DrawRGBPixel(RGB(ccR, ccG, ccB));

					}
				}
            
			}
		}
	}


	}
	//DrawLine(160+50,0,160-50,200);
}




inline void RenderW()
{
	float ccR,ccG,ccB;		// Temp Color
	// Define out fixed FP in space
	float rX = 0; float rY = 0; float rZ = TMS;

	// Main screen loop
	//g_ddsw->Move(int(160 - ((320 / DefRenderStp)/2)), int(100 - ((200 / DefRenderStp)/2)));

	tmp++;
	// This is the point we cast the ray through
	//oX = Xr; oY = Yr; oZ = 0 - TMS;
	// Reset our Point colour..
	ccR = 0;ccG = 0;ccB = 0;
	//TraceAFast(rX, rY, rZ, oX, oY, oZ, ccR, ccG, ccB);
			
	float Div=30,Tms=10;

	// Create pointer to our transformed World sturcture.
	Poly &MyOb = Obz[NOBSz];
	//////////////////////////////////////////////////////////////////////
	// Find closest object : Cast ray from users eye > through screen
	//						 Find closest Polygon that it intersects with.
	//////////////////////////////////////////////////////////////////////
	long OBJ = -1;
	float iX2,iY2;
	for(a = 0; a<NOBSz;a++)
	{
		// Update pointer
		MyOb = Obz[a];	


		for(float aa = 0; aa<=1; aa+=0.05)
		{
		for(float bb = 0; bb<=1; bb+=0.05)
		{
			iX = MyOb.sX + (aa * MyOb.dAx) + (bb * MyOb.dBx);
			iY = MyOb.sY + (aa * MyOb.dAy) + (bb * MyOb.dBy);
			iZ = MyOb.sZ + (aa * MyOb.dAz) + (bb * MyOb.dBz);
			if(iZ>0)
			{
			//iY = oy + (a * (y1 - oy)) + (b * (y2 - oy))
			//iZ = oz + (a * (z1 - oz)) + (b * (z2 - oz))


			iX = ((Tms * iX * (1/(iZ/Div)))/DefRenderStp) + 160;
			iY = ((Tms * iY * (1/(iZ/Div)))/DefRenderStp) + 100;

			//if((iX>0) && (iX<320) && (iY>0) && (iY < 200))
			if((iX>(int(160 - (    (320 / DefRenderStp)/2)))) && (iX<(int(160 + (    (320 / DefRenderStp)/2)))) && (iY>int(100 - ((200 / DefRenderStp)/2))) && (iY < (int(100 + (    (200 / DefRenderStp)/2)))))
			{

					if(MyOb.SurfaceTexture!=-1)
					{
						RGBQUAD pXColor = GetPixel(Texture[MyOb.SurfaceTexture],long((aa*MyOb.SurfaceMultW) * Texture[MyOb.SurfaceTexture].bmWidth), long((bb*MyOb.SurfaceMultH) * Texture[MyOb.SurfaceTexture].bmHeight));
						ccR = BGC_R * ((float(pXColor.rgbRed) /255) * MyOb.Surface_R);
						ccG = BGC_G * ((float(pXColor.rgbGreen) /255) * MyOb.Surface_G);
						ccB = BGC_B * ((float(pXColor.rgbBlue) /255) * MyOb.Surface_R);

					} else {
						// Ok, now add background illumination as a light sorce
						// and scale the whole thing based on the surface color
						ccR = BGC_R * MyOb.Surface_R;
						ccG = BGC_G * MyOb.Surface_G;
						ccB = BGC_B * MyOb.Surface_B;
					}


				g_ddsw->Move(iX,iY);
				ccR=ccR*2.5;
				ccG=ccG*2.5;
				ccB=ccB*2.5;

				g_ddsw->DrawRGBPixel(RGB(ccR, ccG, ccB));
				//DrawLine(iX,iY,iX2,iY2);
			}


			}
		

		}
		}


	}
	//DrawLine(160+50,0,160-50,200);
	RenderTF();
}




inline void DrawTexture()
{
	float Xr,Yr;
	RGBQUAD pXColor;

	g_ddsw->Move(0,0);
	for(Yr=0;Yr<Texture[chk_TextureShow].bmHeight;Yr++)
	{
			for(Xr=0;Xr<Texture[chk_TextureShow].bmWidth;Xr++)
			{
				pXColor = GetPixel(Texture[chk_TextureShow],Xr,Yr);
				g_ddsw->DrawRGBPixel(RGB(pXColor.rgbRed, pXColor.rgbGreen, pXColor.rgbBlue));
			}
			g_ddsw->NextRow();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void TraceA(float &rX,float &rY, float &rZ, float &oX, float &oY, float &oZ, float &ccR, float &ccG, float &ccB)
{

	float cccR=0,cccG=0,cccB=0;	// Temp RGB Values for this function
	float iiX,iiY,iiZ	;		// Result of Ray intersection point.
	float R = 0, lR = 0;		// Temp Len & Ang
	float D;					// Intersect Result.

	// Create pointer to our transformed World sturcture.
	Poly &MyObb = Obz[NOBSz];

	//////////////////////////////////////////////////////////////////////
	// Find closest object : Cast ray from users eye > through screen
	//						 Find closest Polygon that it intersects with.
	//////////////////////////////////////////////////////////////////////
		
	long OBJ = -1;
	for(a = 0; a<NOBSz;a++)
	{
		// Update pointer
		MyObb = Obz[a];	

		// Calculate ray plane intersection point
		D = InterPlane(oX - rX, oY - rY, oZ - rZ, -MyObb.dAx, -MyObb.dAy, -MyObb.dAz, -MyObb.dBx, -MyObb.dBy, -MyObb.dBz, MyObb.sX - oX, MyObb.sY - oY, MyObb.sZ - oZ);
		// D = 0 if ray did not intersect.. iX,iY,iZ are returned from InterPlane() as 
		//									Global floats. If it is less then 0
		//									then it is behind us and we ignor it.
		if((0 != D) && (iZ > 0))
		{
				// Calculate Distance of this point... 	// normaly lR = sqrt((iX * iX) + (iY * iY) + (iZ * iZ));
				lR = iX + iY + iZ;						// but this is faster.
				
				if((lR < R) || (R == 0))				// Compair with last
				{
					R = lR;								// This one is closer,
					iiX = iX; iiY = iY; iiZ = iZ;		//
					tiAt = tiA ; tiBt = tiB;			// tiA,tiB are Global floats returned by
														// InterPlane() as floats. And represent a
														// fraction of intersection .vs. plane surface area.

					OBJ = a;							// O, dont forget to record the poly number itself.
				}
		}
	}


	// Now if OBJ is != -1 then we have a plane intersection so we will now shade that ray..
	if (OBJ != -1)
	{
		// Create another pointer... should use the same one, to our intersected poly.
		const Poly &MyOb = Obz[OBJ];
		if(chk_Lighting)
		{
			// Create pointer to Light Sorce for this loop.
			Light &MyLS = LSt[LSNt];

			// For each light in the scene we will calculate the intensity of light falling
			// on our plane intersection point + Check for shadow casting.
		
			for (long LSX=0;LSX<LSNt;LSX++)
			{
				// Get the light
				MyLS = LSt[LSX];

				float F = 1; // F represents intensity from 0 to 1.  (start with one for point lights)

				if(MyLS.Type==2)
				{
					// Directional Light, not a point light
					//
					// First compute the angle between the light direction and
					// Surface point in relation to the light itself.
					F = -CosAngle(MyLS.Lx - MyLS.Dx, MyLS.Ly - MyLS.Dy, MyLS.Lz - MyLS.Dz, iiX - MyLS.Lx , iiY - MyLS.Ly , iiZ - MyLS.Lz);

					// If the poly point is within the Cone size then
					if((F>=1-MyLS.Cone) && (F<=1))
					{
						// If poly point is in Fuz edge of light then
						if(F <= 1-(MyLS.FuzFactor * MyLS.Cone))
						{
							// Drop light intensity off to 0 at edge of Cone.
							F = (F - (1 - MyLS.Cone)) / ((1 - (MyLS.FuzFactor * MyLS.Cone)) - (1 - MyLS.Cone));
						} else {
							// If it's inside then light will be full beem.
							F=1;
						}
					} else {
						// Out side cone area so light gives no light.
						F = 0;
					}
				}

				// Point light (MyLS.Type==1) : Small dot in space that radiates light in all directions.
				// All we need to do is calculate the angle.

					// Get Cosine of light ray to surface normal,
					// F will = 1 if the angle is 0 and F will = 0 if it is 90deg.
					F = F * CosAngle(MyOb.nX + iiX, MyOb.nY + iiY, MyOb.nZ + iiZ, MyLS.Lx - iiX, MyLS.Ly - iiY, MyLS.Lz - iiZ);
				

				// Calculate Shadows now.. If F is 0 then no need to so we skip it.

				if(F > 0 && F <= 1)
				{

					// If we are Shadowing then we must test each light sorce for
					// Object intersection.
					if(chk_Shadow==true)
					{
						//for(a = 0;a<NOBSt;a++)
						//a = rand()%NOBSt;
						a = MyLS.LastPolyHit;
						int st = a;
loop:
						if(a!=OBJ)
						{
							MyObb = Obt[a];	
							if(0!=InterPlaneEx(MyLS.Lx - iiX, MyLS.Ly - iiY, MyLS.Lz - iiZ, -MyObb.dAx, -MyObb.dAy, -MyObb.dAz, -MyObb.dBx, -MyObb.dBy, -MyObb.dBz, MyObb.sX - MyLS.Lx, MyObb.sY - MyLS.Ly, MyObb.sZ - MyLS.Lz))
							//if(0!=InterPlaneEx(MyLS.Lx - iiX, MyLS.Ly - iiY, MyLS.Lz - iiZ, -Obt[a].dAx, -Obt[a].dAy, -Obt[a].dAz, -Obt[a].dBx, -Obt[a].dBy, -Obt[a].dBz, Obt[a].sX - MyLS.Lx, Obt[a].sY - MyLS.Ly, Obt[a].sZ - MyLS.Lz))
							{
								F = 0;
								MyLS.LastPolyHit = a;
								continue;
							}
						}

						a++;
						if(a==NOBSt) a=0;
						if(a!=st) goto loop;
					}
					
					// Add colour components to our pix value
					F = F * 256;
					cccR = cccR + (MyLS.Colour_R * F);
					cccG = cccG + (MyLS.Colour_G * F);
					cccB = cccB + (MyLS.Colour_B * F);
				}
				
			}
		
		} else {
			cccR = 255;
			cccG = 255;
			cccB = 255;
		}
		if(MyOb.SurfaceTexture!=-1 && chk_Textures)
		{
			RGBQUAD pXColor = GetPixel(Texture[MyOb.SurfaceTexture],long((tiAt*MyOb.SurfaceMultW) * Texture[MyOb.SurfaceTexture].bmWidth), long((tiBt*MyOb.SurfaceMultH) * Texture[MyOb.SurfaceTexture].bmHeight));
			ccR = ccR + (cccR + BGC_R) * ((float(pXColor.rgbRed) /255) * MyOb.Surface_R);
			ccG = ccG + (cccG + BGC_G) * ((float(pXColor.rgbGreen) /255) * MyOb.Surface_G);
			ccB = ccB + (cccB + BGC_B) * ((float(pXColor.rgbBlue) /255) * MyOb.Surface_R);

		} else {
			// Ok, now add background illumination as a light sorce
			// and scale the whole thing based on the surface color
			ccR = ccR + (cccR + BGC_R) * MyOb.Surface_R;
			ccG = ccG + (cccG + BGC_G) * MyOb.Surface_G;
			ccB = ccB + (cccB + BGC_B) * MyOb.Surface_B;
		}

	} else {
		// Looks like sky.
		//RGBQUAD pXColor = GetPixel(Texture[4],oX+160+(360+(-an_Y*2)),oY+100);
		RGBQUAD pXColor = GetPixel(Texture[4],oX+160,oY+100);
		ccR = ccR + float(pXColor.rgbRed);
		ccG = ccG + float(pXColor.rgbGreen);
		ccB = ccB + float(pXColor.rgbBlue);
		
	}
}





inline bool WhatOb(Light &MyLS, float LSiix,float LSiiy,float LSiiz,long &OBJ)
{
	float ObAYBZ,ObAXBY,ObAXBZ,ObAZBY,ObAYBX;
	float MOLSX,MOLSY,MOLSZ;
	float D,D1,D2,D3;
	//Poly &MyObb = Obt[0];
	unsigned int a = MyLS.LastPolyHit;
	if(a>=NOBSt) a=0;
	unsigned int st = a;
loop:
	if(a!=OBJ)
	{
		//MyObb = Obt[a];	
		
		// Things for intersection test.
		ObAYBZ = Obt[a].dAy * Obt[a].dBz;
		ObAXBY = Obt[a].dAx * Obt[a].dBy;
		ObAXBZ = Obt[a].dAx * Obt[a].dBz;
		ObAZBY = Obt[a].dAz * Obt[a].dBy;


		D = LSiix * ObAYBZ + LSiiz * ObAXBY + LSiiy * Obt[a].dBx * Obt[a].dAz - LSiiz * Obt[a].dBx * Obt[a].dAy - LSiix * ObAZBY - LSiiy * ObAXBZ;
		if (D > 0)
		{
			MOLSX = Obt[a].sX - MyLS.Lx;
			MOLSY = Obt[a].sY - MyLS.Ly;
			MOLSZ = Obt[a].sZ - MyLS.Lz;

			D2 = (-Obt[a].dBz * MOLSY * LSiix - Obt[a].dBy * MOLSX * LSiiz - Obt[a].dBx * LSiiy * MOLSZ + Obt[a].dBx * MOLSY * LSiiz + Obt[a].dBy * MOLSZ * LSiix + Obt[a].dBz * MOLSX * LSiiy)/D;
			if ((D2 >= 0) && (D2 <= 1))
			{
				D3 = (-Obt[a].dAy * MOLSZ * LSiix - Obt[a].dAx * MOLSY * LSiiz - Obt[a].dAz * LSiiy * MOLSX + Obt[a].dAy * MOLSX * LSiiz + Obt[a].dAz * MOLSY * LSiix + Obt[a].dAx * MOLSZ * LSiiy)/D;
				if ((D3 >= 0) && (D3 <= 1))
				{
					ObAYBX = Obt[a].dAy * Obt[a].dBx;
					D1 = (ObAYBZ * MOLSX + ObAXBY * MOLSZ + Obt[a].dBx * MOLSY * Obt[a].dAz - ObAYBX * MOLSZ - ObAZBY * MOLSX - ObAXBZ * MOLSY) / D;
					if ((D1 < 0) && (D1 > -1))
					{				
						MyLS.LastPolyHit = a;
						return true;
					}
				}
			}
			
		}

	}

	a++;
	if(a==NOBSt) a=0;
	if(a!=st) goto loop;

	//MyLS.LastPolyHit = 0;
	return false;

}



inline void TraceAFast(float &rX,float &rY, float &rZ, float &oX, float &oY, float &oZ, float &ccR, float &ccG, float &ccB)
{

	float cccR=0,cccG=0,cccB=0;	// Temp RGB Values for this function
	float iiX,iiY,iiZ	;		// Result of Ray intersection point.
	float R = 10000000, lR = 0;		// Temp Len & Ang
	//float D;					// Intersect Result.

	// Create pointer to our transformed World sturcture.
	Poly &MyObb = Obz[NOBSz];

	//////////////////////////////////////////////////////////////////////
	// Find closest object : Cast ray from users eye > through screen
	//						 Find closest Polygon that it intersects with.
	//////////////////////////////////////////////////////////////////////
		
	long OBJ = -1;
	for(a = 0; a<NOBSz;a++)
	{
		// Update pointer
		MyObb = Obz[a];	

		// InterPlane :: Calculate ray plane intersection point
		// D = 0 if ray did not intersect.. iX,iY,iZ are returned from InterPlane() as 
		//									Global floats. If it is less then 0
		//									then it is behind us and we ignor it.
		if(0 != InterPlane(oX - rX, oY - rY, oZ - rZ, -MyObb.dAx, -MyObb.dAy, -MyObb.dAz, -MyObb.dBx, -MyObb.dBy, -MyObb.dBz, MyObb.sX - oX, MyObb.sY - oY, MyObb.sZ - oZ))
		{
				// Calculate Distance of this point... 	// normaly lR = sqrt((iX * iX) + (iY * iY) + (iZ * iZ));
				lR = (iX * iX) + (iY * iY) + (iZ * iZ);						// but this is faster.
				
				if((lR < R))							// Compair with last
				{
					R = lR;								// This one is closer,
					iiX = iX; iiY = iY; iiZ = iZ;		//
					tiAt = tiA ; tiBt = tiB;			// tiA,tiB are Global floats returned by
														// InterPlane() as floats. And represent a
														// fraction of intersection .vs. plane surface area.

					OBJ = a;							// O, dont forget to record the poly number itself.
				}
		}
	}


	// Now if OBJ is != -1 then we have a plane intersection so we will now shade that ray..
	if (OBJ != -1)
	{
		// Create another pointer... should use the same one, to our intersected poly.
		const Poly &MyOb = Obz[OBJ];
		if(chk_Lighting)
		{
			// Create pointer to Light Sorce for this loop.
			Light &MyLS = LSt[LSNt];

			// For each light in the scene we will calculate the intensity of light falling
			// on our plane intersection point + Check for shadow casting.
		
			for (long LSX=0;LSX<LSNt;LSX++)
			{
				// Get the light
				MyLS = LSt[LSX];

				float F = 1; // F represents intensity from 0 to 1.  (start with one for point lights)

				if(MyLS.Type==2)
				{
					// Directional Light, not a point light
					//
					// First compute the angle between the light direction and
					// Surface point in relation to the light itself.
					F = -CosAngle(MyLS.Lx - MyLS.Dx, MyLS.Ly - MyLS.Dy, MyLS.Lz - MyLS.Dz, iiX - MyLS.Lx , iiY - MyLS.Ly , iiZ - MyLS.Lz);

					// If the poly point is within the Cone size then
					if((F>=1-MyLS.Cone) && (F<=1))
					{
						// If poly point is in Fuz edge of light then
						if(F <= 1-(MyLS.FuzFactor * MyLS.Cone))
						{
							// Drop light intensity off to 0 at edge of Cone.
							F = (F - (1 - MyLS.Cone)) / ((1 - (MyLS.FuzFactor * MyLS.Cone)) - (1 - MyLS.Cone));
						} else {
							// If it's inside then light will be full beem.
							F=1;
						}
					} else {
						// Out side cone area so light gives no light.
						F = 0;
					}
				}

				// Point light (MyLS.Type==1) : Small dot in space that radiates light in all directions.
				// All we need to do is calculate the angle.

					// Get Cosine of light ray to surface normal,
					// F will = 1 if the angle is 0 and F will = 0 if it is 90deg.
					F = F * CosAngle(MyOb.nX + iiX, MyOb.nY + iiY, MyOb.nZ + iiZ, MyLS.Lx - iiX, MyLS.Ly - iiY, MyLS.Lz - iiZ);
				

				// Calculate Shadows now.. If F is 0 then no need to so we skip it.

				if(F > 0 && F <= 1)
				{

					// If we are Shadowing then we must test each light sorce for
					// Object intersection.
					if(chk_Shadow==true)
					{

					if(WhatOb(MyLS,MyLS.Lx - iiX,MyLS.Ly - iiY,MyLS.Lz - iiZ,OBJ)) continue;
					}
					// Add colour components to our pix value
					F = F * 256;
					cccR = cccR + (MyLS.Colour_R * F);
					cccG = cccG + (MyLS.Colour_G * F);
					cccB = cccB + (MyLS.Colour_B * F);
					
				}
				
			}
		
		} else {
			cccR = 255;
			cccG = 255;
			cccB = 255;
		}

		if(MyOb.SurfaceTexture!=-1 && chk_Textures)
		{
			RGBQUAD pXColor = GetPixel(Texture[MyOb.SurfaceTexture],long((tiAt*MyOb.SurfaceMultW) * Texture[MyOb.SurfaceTexture].bmWidth), long((tiBt*MyOb.SurfaceMultH) * Texture[MyOb.SurfaceTexture].bmHeight));
			ccR = ccR + (cccR + BGC_R) * ((float(pXColor.rgbRed) /255) * MyOb.Surface_R);
			ccG = ccG + (cccG + BGC_G) * ((float(pXColor.rgbGreen) /255) * MyOb.Surface_G);
			ccB = ccB + (cccB + BGC_B) * ((float(pXColor.rgbBlue) /255) * MyOb.Surface_R);

		} else {
			// Ok, now add background illumination as a light sorce
			// and scale the whole thing based on the surface color
			ccR = ccR + (cccR + BGC_R) * MyOb.Surface_R;
			ccG = ccG + (cccG + BGC_G) * MyOb.Surface_G;
			ccB = ccB + (cccB + BGC_B) * MyOb.Surface_B;
		}



	} else {
		// Looks like sky.
		//RGBQUAD pXColor = GetPixel(Texture[4],oX+160+(360+(-an_Y*2)),oY+100);

		RGBQUAD pXColor = GetPixel(Texture[4],((360-an_Y)*4) + oX+161,oY+100);
		ccR = ccR + float(pXColor.rgbRed);
		ccG = ccG + float(pXColor.rgbGreen);
		ccB = ccB + float(pXColor.rgbBlue);
		
	}

	// Spooky.. 
	//ccR = ccR * (255/sqrt(R));
	//ccG = ccG * (255/sqrt(R));
	//ccB = ccB * (255/sqrt(R));


}



/*
inline void TraceAFast(float &rX,float &rY, float &rZ, float &oX, float &oY, float &oZ, float &ccR, float &ccG, float &ccB)
{

	float cccR=0,cccG=0,cccB=0;	// Temp RGB Values for this function
	float iiX,iiY,iiZ	;		// Result of Ray intersection point.
	float R = 0, lR = 0;		// Temp Len & Ang
	float D;					// Intersect Result.

	// Create pointer to our transformed World sturcture.
	Poly &MyObb = Obz[NOBSz];

	//////////////////////////////////////////////////////////////////////
	// Find closest object : Cast ray from users eye > through screen
	//						 Find closest Polygon that it intersects with.
	//////////////////////////////////////////////////////////////////////
	long OBJ = -1;
	for(a = 0; a<NOBSz;a++)
	{
		// Update pointer
		MyObb = Obz[a];	

		// Calculate ray plane intersection point
		D = InterPlane(oX - rX, oY - rY, oZ - rZ, -MyObb.dAx, -MyObb.dAy, -MyObb.dAz, -MyObb.dBx, -MyObb.dBy, -MyObb.dBz, MyObb.sX - oX, MyObb.sY - oY, MyObb.sZ - oZ);
		// D = 0 if ray did not intersect.. iX,iY,iZ are returned from InterPlane() as 
		//									Global floats. If it is less then 0
		//									then it is behind us and we ignor it.
		if((0 != D) && (iZ > 0))
		{
				// Calculate Distance of this point... 	// normaly lR = sqrt((iX * iX) + (iY * iY) + (iZ * iZ));
				lR = iX + iY + iZ;						// but this is faster.
				
				if((lR < R) || (R == 0))				// Compair with last
				{
					R = lR;								// This one is closer,
					iiX = iX; iiY = iY; iiZ = iZ;		//
					tiAt = tiA ; tiBt = tiB;			// tiA,tiB are Global floats returned by
														// InterPlane() as floats. And represent a
														// fraction of intersection .vs. plane surface area.

					OBJ = a;							// O, dont forget to record the poly number itself.
				}

		}
	}


	// Now if OBJ is != -1 then we have a plane intersection so we will now shade that ray..
	if (OBJ != -1)
	{
		// Create another pointer... should use the same one, to our intersected poly.
		const Poly &MyOb = Obz[OBJ];
		// Create pointer to Light Sorce for this loop.
		Light &MyLS = LSt[LSNt];

		// For each light in the scene we will calculate the intensity of light falling
		// on our plane intersection point + Check for shadow casting.

		bool LSu[100];
		long LSuCnt=0;
		for(a=0;a<LSNt;a++)
			LSu[a] = true;

		LSuCnt = LSNt;

		for(a = 0;a<NOBSt;a++)
		{
			if(a != OBJ)
			{	
				for (long LSX=0;LSX<LSNt;LSX++)
				{
					if(LSu[LSX]==true)
					{	
						MyLS = LSt[LSX];

						if(0!=InterPlaneEx(MyLS.Lx - iiX, MyLS.Ly - iiY, MyLS.Lz - iiZ, -Obt[a].dAx, -Obt[a].dAy, -Obt[a].dAz, -Obt[a].dBx, -Obt[a].dBy, -Obt[a].dBz, Obt[a].sX - MyLS.Lx, Obt[a].sY - MyLS.Ly, Obt[a].sZ - MyLS.Lz))
						{
							LSu[LSX] = false;
							LSuCnt--;
							if(LSuCnt<1) break;
						}
					}
				}
			}
			if(LSuCnt<1) break;
		}

		for (long LSX=0;LSX<LSNt;LSX++)
		{
			if(LSu[LSX]==true)
			{	
				// Get the light
				MyLS = LSt[LSX];

				float F = 1; // F represents intensity from 0 to 1.  (start with one for point lights)

				if(MyLS.Type==2)
				{
					// Directional Light, not a point light
					//
					// First compute the angle between the light direction and
					// Surface point in relation to the light itself.
					F = -CosAngle(MyLS.Lx - MyLS.Dx, MyLS.Ly - MyLS.Dy, MyLS.Lz - MyLS.Dz, iiX - MyLS.Lx , iiY - MyLS.Ly , iiZ - MyLS.Lz);

					// If the poly point is within the Cone size then
					if((F>=1-MyLS.Cone) && (F<=1))
					{
						// If poly point is in Fuz edge of light then
						if(F <= 1-(MyLS.FuzFactor * MyLS.Cone))
						{
							// Drop light intensity off to 0 at edge of Cone.
							F = (F - (1 - MyLS.Cone)) / ((1 - (MyLS.FuzFactor * MyLS.Cone)) - (1 - MyLS.Cone));
						} else {
							// If it's inside then light will be full beem.
							F=1;
						}
					} else {
						// Out side cone area so light gives no light.
						F = 0;
					}
				}

				// Point light (MyLS.Type==1) : Small dot in space that radiates light in all directions.
				// All we need to do is calculate the angle.

				// Get Cosine of light ray to surface normal,
				// F will = 1 if the angle is 0 and F will = 0 if it is 90deg.
				F = F * CosAngle(MyOb.nX + iiX, MyOb.nY + iiY, MyOb.nZ + iiZ, MyLS.Lx - iiX, MyLS.Ly - iiY, MyLS.Lz - iiZ);
				

				// Calculate Shadows now.. If F is 0 then no need to so we skip it.

				// Add colour components to our pix value
				F = F * 256;
				cccR = cccR + (MyLS.Colour_R * F);
				cccG = cccG + (MyLS.Colour_G * F);
				cccB = cccB + (MyLS.Colour_B * F);
			}
		}


		if(MyOb.SurfaceTexture!=-1 && chk_Textures)
		{
			RGBQUAD pXColor = GetPixel(Texture[MyOb.SurfaceTexture],long((tiAt*MyOb.SurfaceMultW) * Texture[MyOb.SurfaceTexture].bmWidth), long((tiBt*MyOb.SurfaceMultH) * Texture[MyOb.SurfaceTexture].bmHeight));
			ccR = ccR + (cccR + BGC_R) * ((float(pXColor.rgbRed) /255) * MyOb.Surface_R);
			ccG = ccG + (cccG + BGC_G) * ((float(pXColor.rgbGreen) /255) * MyOb.Surface_G);
			ccB = ccB + (cccB + BGC_B) * ((float(pXColor.rgbBlue) /255) * MyOb.Surface_R);

		} else {
			// Ok, now add background illumination as a light sorce
			// and scale the whole thing based on the surface color
			ccR = ccR + (cccR + BGC_R) * MyOb.Surface_R;
			ccG = ccG + (cccG + BGC_G) * MyOb.Surface_G;
			ccB = ccB + (cccB + BGC_B) * MyOb.Surface_B;
		}

	} else {
		// Looks like sky.
		//RGBQUAD pXColor = GetPixel(Texture[4],oX+160+(360+(-an_Y*2)),oY+100);
		RGBQUAD pXColor = GetPixel(Texture[4],oX+160,oY+100);
		ccR = ccR + float(pXColor.rgbRed);
		ccG = ccG + float(pXColor.rgbGreen);
		ccB = ccB + float(pXColor.rgbBlue);
		
	}
}
*/


////////////////////////////////////////////////////////////
//// GetPixel : Returns RGB from MyTexture.bmpBuffer @ x,y
////////////////////////////////////////////////////////////
inline RGBQUAD GetPixel(cTexture &MyTexture, long x, long y)
{
	static RGBQUAD pixColor = {0, 0, 0, 0};
	
	// Next two lines perform wrap around. See MyTexture..SurfaceMultH and SurfaceMultW
	if(x > MyTexture.bmWidth) x = MyTexture.bmWidth * (x/double(MyTexture.bmWidth) - floor(x/MyTexture.bmWidth));
	if(y > MyTexture.bmHeight) y = MyTexture.bmHeight * (y/double(MyTexture.bmHeight) - floor(y/MyTexture.bmHeight));

	// Create pointer to start of bmpBuffer
	BYTE * dibits = MyTexture.bmpBuffer;
	// Calculate Offset given x and y
    dibits += (x * 3) + (y * (MyTexture.bmWidth*3));
	// Perform Corrections.. I dont know why, some 24BMP's have an extra byte at the
	//							end of each line.
	if(MyTexture.bmWidth*3==MyTexture.bmWidthBytes-1) dibits += y;
    if(MyTexture.bmWidth*3==MyTexture.bmWidthBytes-3) dibits += y;
	// Set RGBQUAD instance up and return it.
    pixColor.rgbBlue = dibits[0];
	pixColor.rgbGreen = dibits[1];
	pixColor.rgbRed = dibits[2];
	return pixColor;
}


bool LoadTexture(cTexture &MyTexture, LPTSTR szFileName)
{
	BITMAP        bm;		   	
	HBITMAP       hBitmap;
	HPALETTE      hPalette;
	if( LoadBitmapFromBMPFile( szFileName, &hBitmap, &hPalette ) )
	{
      GetObject( hBitmap, sizeof(BITMAP), &bm );
	  MyTexture.bmBitsPixel = bm.bmBitsPixel;
	  MyTexture.bmHeight = bm.bmHeight;
	  MyTexture.bmPlanes = bm.bmPlanes;
	  MyTexture.bmType = bm.bmType;
	  MyTexture.bmWidth = bm.bmWidth;
	  MyTexture.bmWidthBytes = bm.bmWidthBytes;
	  MyTexture.bmpBuffer=(BYTE*)GlobalAlloc(GPTR,bm.bmWidthBytes*bm.bmHeight);//
	  GlobalLock(MyTexture.bmpBuffer);
      if(MyTexture.bmBitsPixel==24)
		GetBitmapBits(hBitmap,bm.bmWidthBytes*bm.bmHeight,MyTexture.bmpBuffer);

 	  GlobalUnlock((HGLOBAL)Texture[a].bmpBuffer);
	  GlobalFree((HGLOBAL)Texture[a].bmpBuffer);//Free memory ;

      DeleteObject( hBitmap );
      DeleteObject( hPalette );
	  return true;
	} else {
		return false;
	}
}


bool LoadBitmapFromBMPFile( LPTSTR szFileName, HBITMAP *phBitmap,HPALETTE *phPalette )
{
   BITMAP  bm;

   *phBitmap = NULL;
   *phPalette = NULL;

   // Use LoadImage() to get the image loaded into a DIBSection
   *phBitmap = (HBITMAP)LoadImage( NULL, szFileName, IMAGE_BITMAP, 0, 0,
               LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );
   if( *phBitmap == NULL )
     return FALSE;

   // Get the color depth of the DIBSection
   GetObject(*phBitmap, sizeof(BITMAP), &bm );
   // If the DIBSection is 256 color or less, it has a color table
	/*
   if( ( bm.bmBitsPixel * bm.bmPlanes ) <= 8 )
   {
   HDC           hMemDC;
   HBITMAP       hOldBitmap;
   RGBQUAD       rgb[256];
   LPLOGPALETTE  pLogPal;
   WORD          i;

   // Create a memory DC and select the DIBSection into it
   hMemDC = CreateCompatibleDC( NULL );
   hOldBitmap = (HBITMAP)SelectObject( hMemDC, *phBitmap );
   // Get the DIBSection's color table
   GetDIBColorTable( hMemDC, 0, 256, rgb );
   // Create a palette from the color tabl
   pLogPal = (LOGPALETTE *)malloc( sizeof(LOGPALETTE) + (256*sizeof(PALETTEENTRY)) );
   pLogPal->palVersion = 0x300;
   pLogPal->palNumEntries = 256;
   for(i=0;i<256;i++)
   {
     pLogPal->palPalEntry[i].peRed = rgb[i].rgbRed;
     pLogPal->palPalEntry[i].peGreen = rgb[i].rgbGreen;
     pLogPal->palPalEntry[i].peBlue = rgb[i].rgbBlue;
     pLogPal->palPalEntry[i].peFlags = 0;
   }
   *phPalette = CreatePalette( pLogPal );
   // Clean up
   free( pLogPal );
   SelectObject( hMemDC, hOldBitmap );
   DeleteDC( hMemDC );
   }
   else   // It has no color table, so use a halftone palette
   {
   */
   HDC    hRefDC;

   hRefDC = GetDC( NULL );
   *phPalette = CreateHalftonePalette( hRefDC );
   ReleaseDC( NULL, hRefDC );
   //}
   return TRUE;

}


