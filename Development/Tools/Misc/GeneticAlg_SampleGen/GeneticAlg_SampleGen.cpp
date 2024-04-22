/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "stdafx.h"
#include "GeneticAlg_SampleGen.h"
#include "TGeneticAlgorithm.h"
#include "GAInstance_Test.h"
#include "TGeneticAlgorithm.h"

#define MAX_LOADSTRING 100

HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name


#define IN_SPHERE				1				// 0=on sphere or 1=in sphere
#define IN_SPHERE_POW			0.5f			// 1 for linear distribution, >1 for more points to the center, <1 but >0 more points to the oouter area

#define POP_MULTIPLY			9				// 1 super fast, 10 for human mode, 1000 for over night
#define TIME_MULTIPLY			10				// 1 super fast, 10 for human mode, 1000 for over night

// best so far:
// POP_MULTIPLY:9 TIME_MULTIPLY:10 IN_SPHERE:1 IN_SPHERE_POW:0.5f
// Best -6.939256 Best -27.298071

DWORD g_dwWidth=32, g_dwHeight=32;

const DWORD g_dwBestSamplePos = 8;
vec3 g_vBestSamplePos[g_dwBestSamplePos];

const DWORD g_dwBestMirrorVec = 16;
vec3 g_vBestMirrorVec[g_dwBestMirrorVec];



ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
void Draw( HDC hdc );




int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_GENETICALG_SAMPLEGEN, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	srand(23222);

/*
//	TGeneticAlgorithm<CGAInstance> Algorithm(5,2,0.1f);		// Individuals, ChilrenPerStep, MutationRate
	TGeneticAlgorithm<CGAInstance_Test> Algorithm(50,5,0.1f);

	for(int i=0;i<200;++i)
	{
		Algorithm.Step(false);
	}
	*/
	

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GENETICALG_SAMPLEGEN));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GENETICALG_SAMPLEGEN));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_GENETICALG_SAMPLEGEN);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      20,20,800,700, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		Draw(hdc);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}






void Init(){}
void DeInit();







void Draw( HDC hdc );


DWORD g_dwCursorX=0;

float ComputeWeight( const DWORD dwSampleI, const DWORD dwSampleCount )
{
#if IN_SPHERE==1
	return (float)powf((dwSampleI+1)/(float)dwSampleCount,IN_SPHERE_POW);
#else
	return 1.0f;
#endif
}



// smaller means nearer
float DistanceMetric( const vec3 &vSampleA, const vec3 &vSampleB, const float fLocationA, const float fLocationB )
{
	return (vSampleA*fLocationA-vSampleB*fLocationB).length();
}

/*
float MinDist( const vec3 *pData, DWORD dwCnt, DWORD dwWorstId[2] )
{
	float ret2=FLT_MAX;

	dwWorstId[0]=0;
	dwWorstId[1]=0;

	for(DWORD dwA=0;dwA<dwCnt;++dwA)
	for(DWORD dwB=dwA+1;dwB<dwCnt;++dwB)
	{
		float fDist2 = DistanceMetric(pData[dwA],pData[dwB],ComputeWeight(dwA,dwCnt),ComputeWeight(dwB,dwCnt));

		if(fDist2<ret2)
		{
			ret2 = fDist2;
			dwWorstId[0]=dwA;
			dwWorstId[1]=dwB;
		}
	}

	return (float)sqrt(ret2);
}
*/

// smaller means better
float FitnessFunc( const vec3 *pData, DWORD dwCnt )
{
	float ret = 0;

	for(DWORD dwA=0;dwA<dwCnt;++dwA)
	{
		float Nearest2 = FLT_MAX;

		for(DWORD dwB=0;dwB<dwCnt;++dwB)
		{
			if(dwA==dwB)
				continue;

			float fDist2 = DistanceMetric(pData[dwA],pData[dwB],ComputeWeight(dwA,dwCnt),ComputeWeight(dwB,dwCnt));

			if(fDist2<Nearest2)
			{
				Nearest2 = fDist2;
			}
		}

//		char str[256];
//		sprintf_s(str,sizeof(str),"%f ",Nearest2);
//		OutputDebugString(str);

		if(Nearest2 != FLT_MAX)
			ret -= Nearest2;
	}

//	char str[256];
//	sprintf_s(str,sizeof(str),"= %f\n",ret);
//	OutputDebugString(str);

	return ret;
}

/*
// compute a quality value for a given vMirrorTest mirror vector against a sample set mirrored with a set of mirror vectors
// pMirrorData - normalized
// vMirrorTest - normalized
// return smaller is better
float MinDistMirroredSamples( vec3 *pSampleData, DWORD dwSampleCnt, vec3 *pMirrorData, DWORD dwMirrorCnt, const vec3 &vMirrorTest )
{
	float ret2 = 0;

	for(DWORD dwA=0;dwA<dwSampleCnt;++dwA)
	{
		float fNearest2=FLT_MAX;

		vec3 vSampleA = mirror(pSampleData[dwA],vMirrorTest);

		for(DWORD dwSampleI=0;dwSampleI<dwSampleCnt;++dwSampleI)
		for(DWORD dwMirrorI=0;dwMirrorI<dwMirrorCnt;++dwMirrorI)
		{
			vec3 vSampleB = mirror(pSampleData[dwSampleI],pMirrorData[dwMirrorI]);

//			float fDist2 = (vSampleA-vSampleB).length2();
			float fDist2 = DistanceMetric(vSampleA,vSampleB,dwA/(dwSampleCnt-1.0f),dwSampleI/(dwSampleCnt-1.0f));

			assert(!_isnan(fDist2));

			if(fDist2<fNearest2)
				fNearest2 = fDist2;
		}

		ret2 += fNearest2;

		assert(!_isnan(fNearest2));
	}

	assert(!_isnan(ret2));

	return ret2;
}
*/


// compute a quality value for a given vMirrorTest mirror vector against a sample set mirrored with a set of mirror vectors
// pMirrorData - normalized
// return smaller is better
float FitnessFuncMirroredSamples( const vec3 *pSampleData, DWORD dwSampleCnt, const vec3 *pMirrorData, const DWORD dwMirrorCnt )
{
	float ret2 = 0;

	for(DWORD dwSampleE = 0; dwSampleE < dwSampleCnt; ++dwSampleE)
	{
		for(DWORD dwMirrorE = 0; dwMirrorE < dwMirrorCnt; ++dwMirrorE)
		{
			float fNearest2 = FLT_MAX;

			vec3 vSampleA = mirror(pSampleData[dwSampleE], pMirrorData[dwMirrorE]);

			for(DWORD dwSampleI=0;dwSampleI<dwSampleCnt;++dwSampleI)
			{
				for(DWORD dwMirrorI=0;dwMirrorI<dwMirrorCnt;++dwMirrorI)
				{
					if(dwSampleE == dwSampleI && dwMirrorI==dwMirrorE)
						continue;

					vec3 vSampleB = mirror(pSampleData[dwSampleI],pMirrorData[dwMirrorI]);

					float fDist2 = DistanceMetric(vSampleA,vSampleB,dwSampleE/(dwSampleCnt-1.0f),dwSampleI/(dwSampleCnt-1.0f));

					assert(!_isnan(fDist2));

					if(fDist2<fNearest2)
						fNearest2 = fDist2;
				}
			}

			ret2 -= fNearest2;

			assert(!_isnan(fNearest2));
		}
	}

	assert(!_isnan(ret2));

	return ret2;
}

void SubPixel( HDC hdc, int x, int y, int rel )
{
	DWORD col = GetPixel(hdc,x,y)&0xff;

	if(col>(DWORD)rel)
		col-=rel;
	  else
		col=0;

	SetPixel(hdc,x,y,(col<<16)|(col<<8)|col);
}

void SubPixel2( HDC hdc, int x, int y, int rel )
{
	SubPixel(hdc,x,y,rel*2);
	SubPixel(hdc,x-1,y,rel);
	SubPixel(hdc,x+1,y,rel);
	SubPixel(hdc,x,y-1,rel);
	SubPixel(hdc,x,y+1,rel);
}


void DrawSphereCircles( HDC hdc, int xx, int yy, DWORD rr )
{
	for(float f=0;f<6.28f;f+=0.01f)
	{
		int x,y;

		x = (int)(xx+(sin(f)*0.5f+0.5f)*rr);
		y = (int)(yy+(cos(f)*0.5f+0.5f)*rr);

		SetPixel(hdc,x,y,0x888888);
		SetPixel(hdc,x,y+rr,0x888888);
	}
}


void DrawSamples( HDC hdc, int xx, int yy, DWORD rr, const vec3 *pSampleData, const DWORD dwSampleCnt, int rel )
{
	DrawSphereCircles(hdc,xx,yy,rr);
	
	for(DWORD dwI=0;dwI<dwSampleCnt;++dwI)
	{
		int x,y;

		float fLocation = ComputeWeight(dwI,dwSampleCnt);

		x = (int)(xx+(pSampleData[dwI].x*fLocation*0.5f+0.5f)*rr);
		y = (int)(yy+(pSampleData[dwI].y*fLocation*0.5f+0.5f)*rr);

		SubPixel2(hdc,x,y,rel);

		y = (int)(yy+(pSampleData[dwI].z*0.5f+0.5f)*rr);

		SubPixel2(hdc,x,y+rr,rel);
	}
}


void DrawMirroredSamples( HDC hdc, int xx, int yy, DWORD rr,
	const vec3 *pSampleData, const DWORD dwSampleCnt,
	const vec3 *pMirrorData, DWORD dwMirrorCnt,
	int rel )
{
	for(DWORD dwMirrorI=0;dwMirrorI<dwMirrorCnt;++dwMirrorI)
	{
		vec3 vDrawSamples[32];			assert(dwSampleCnt<32);

		for(DWORD dwSampleI=0;dwSampleI<dwSampleCnt;++dwSampleI)
		{
			float fLocation = ComputeWeight(dwSampleI,dwSampleCnt);

			vDrawSamples[dwSampleI] = mirror(pSampleData[dwSampleI]*fLocation,pMirrorData[dwMirrorI]);
		}

		DrawSamples(hdc,xx,yy,rr,vDrawSamples,dwSampleCnt,64);
	}
}


void DrawDensityHistogram( HDC hdc, int xx, int yy, DWORD rr, const vec3 *pSampleData, const DWORD dwSampleCnt )
{
	const DWORD dwHistCnt=50;

	float fHistogram[dwHistCnt];

	for(DWORD dwI=0;dwI<dwHistCnt;++dwI)
		fHistogram[dwI]=0;


	// build histogram from center outwards 
	for(DWORD dwA=0;dwA<dwSampleCnt;++dwA)
	{
		float fLen = pSampleData[dwA].length();

//		assert(fLen<1.0f/0.999f);
		if(fLen>1.0f)
			fLen=1.0f;

		fLen *= ComputeWeight(dwA,dwSampleCnt);

		DWORD dwBucket = (DWORD)(fLen*dwHistCnt*0.9999f);

		assert(dwBucket<dwHistCnt);

		++fHistogram[dwBucket];
	}

/*		
	// build histogram from center outwards 
	for(DWORD dwI=0;dwI<1000;++dwI)
	{
		vec3 vDir = frand_PointOnUnitSphere();
		for(DWORD dwA=0;dwA<dwSampleCnt;++dwA)
		{
			float fLen = (float)fabs(dot(pSampleData[dwA],vDir));

			fLen *= ComputeWeight(dwA,dwSampleCnt);

			DWORD dwBucket = (DWORD)(fLen*dwHistCnt*0.95f);		

			assert(dwBucket<dwHistCnt);

			++fHistogram[dwBucket];
		}
	}
*/
	// normalize
	{
		float fSum=0;

		for(DWORD dwI=0;dwI<dwHistCnt;++dwI)
			fSum+=fHistogram[dwI];

		for(DWORD dwI=0;dwI<dwHistCnt;++dwI)
			fHistogram[dwI]/=fSum;
	}

	for(DWORD dwX=0;dwX<rr;++dwX)
	for(DWORD dwY=0;dwY<rr;++dwY)
	{
		DWORD dwBucket = dwX*dwHistCnt/rr;
		float fRef = 1.0f-dwY/(float)rr;
		SetPixel(hdc,xx+dwX,yy+dwY+2*rr,fRef<fHistogram[dwBucket]?0x00ff00:0xddffdd);		// green
	}
}



void DrawPairDistanceHistogram( HDC hdc, int xx, int yy, DWORD rr, const vec3 *pSampleData, const DWORD dwSampleCnt )
{
	const DWORD dwHistCnt=50;

	float fHistogram[dwHistCnt];

	for(DWORD dwI=0;dwI<dwHistCnt;++dwI)
		fHistogram[dwI]=0;

	// build histogram from pair distances
	for(DWORD dwA=0;dwA<dwSampleCnt;++dwA)
		for(DWORD dwB=dwA+1;dwB<dwSampleCnt;++dwB)
		{
			vec3 vSampleA = pSampleData[dwA];
			vec3 vSampleB = pSampleData[dwB];

			vSampleA = vSampleA * ComputeWeight(dwA,dwSampleCnt);
			vSampleB = vSampleB * ComputeWeight(dwB,dwSampleCnt);

			float fLen = (vSampleA-vSampleB).length()*0.5f;

			DWORD dwBucket = (DWORD)(fLen*dwHistCnt*0.9999f);		

			assert(dwBucket<dwHistCnt);

			++fHistogram[dwBucket];
		}
		
		// normalize
		{
			float fSum=0;

			for(DWORD dwI=0;dwI<dwHistCnt;++dwI)
				fSum+=fHistogram[dwI];

			for(DWORD dwI=0;dwI<dwHistCnt;++dwI)
				fHistogram[dwI]/=fSum;
		}

		for(DWORD dwX=0;dwX<rr;++dwX)
			for(DWORD dwY=0;dwY<rr;++dwY)
			{
				DWORD dwBucket = dwX*dwHistCnt/rr;
				float fRef = 1.0f-dwY/(float)rr;
				SetPixel(hdc,xx+dwX,yy+dwY+2*rr,fRef<fHistogram[dwBucket]?0xff0000:0xffdddd);		// blue
			}
}



template <int TCount>
class CGAInstance_SphereSamples
{
public:

	void SetRandomInitalState()
	{
		for(int i = 0; i < TCount; ++i)
		{
			vSamples[i] = frand_PointOnUnitSphere();
			assert(vSamples[i].length()<1.0f/0.999f);
		}
	}

	float *GetDataAccess( DWORD &OutCount )
	{
		OutCount = TCount*3;

		return (float *)this;
	}

	void Renormalize()
	{
		for(int i = 0; i < TCount; ++i)
		{
			vSamples[i] = vSamples[i].UnsafeNormal();
//			float Len = vSamples[i].length();
	
//			vSamples[i] *= ComputeWeight(i,TCount)/Len;

//			float Len2 = vSamples[i].length();
//			assert(Len2<1.0f/0.999f);
		}
	}

	void Debug()
	{
		for(int i = 0; i < TCount; ++i)
		{
			float Len = vSamples[i].length();
			assert(Len<1.0f/0.999f);
		}

		/*
		char str[256];

		for(int i = 0; i < TCount; ++i)
		{
			sprintf_s(str,sizeof(str),"(%.2f|.2%f|.2%f) ",vSamples[i].x,vSamples[i].y,vSamples[i].z);
			OutputDebugString(str);
		}
		OutputDebugString("\n");*/
	}

	// smaller means better
	float ComputeFitnessValue() const
	{
		return FitnessFunc(vSamples,TCount);
	}

//private: // -------------------------------------

	vec3		vSamples[TCount];
};

template <int TCount>
void GA_FindGoodRandomPointsOnASphere( HDC hdc )
{
	TGeneticAlgorithm<CGAInstance_SphereSamples<TCount> > Algorithm(2000*POP_MULTIPLY,100*POP_MULTIPLY,0.003f);

	DrawSphereCircles(hdc,g_dwCursorX,0,150);

	for(DWORD dwI = 0; dwI < 4*240*TIME_MULTIPLY; ++dwI)
	{
		/*
		vec3 vSrcSamples[8] = 
		{
			vec3(1, 0, 0.594603557501360532f).UnsafeNormal(),
			vec3(-1, 0, 0.594603557501360532f).UnsafeNormal(),
			vec3(0, 1, 0.594603557501360532f).UnsafeNormal(),
			vec3(0, -1, 0.594603557501360532f).UnsafeNormal(),
			vec3(0.707106781186547525f, 0.707106781186547525f, -0.594603557501360532f).UnsafeNormal(),
			vec3(0.707106781186547525f, -0.707106781186547525f, -0.594603557501360532f).UnsafeNormal(),
			vec3(-0.707106781186547525f, 0.707106781186547525f, -0.594603557501360532f).UnsafeNormal(),
			vec3(-0.707106781186547525f, -0.707106781186547525f, -0.594603557501360532f).UnsafeNormal()
		};
//		if(dwI==1)
		memcpy((void *)(Algorithm.GetBest().vSamples),vSrcSamples,8*4*3);
		Algorithm.UpdateBestFitnessValue();
*/

		bool bBetterOneFound = Algorithm.Step(false);
//		DrawSamples(hdc,g_dwCursorX,0,150,Algorithm.GetBest().vSamples,TCount,4);

		if(bBetterOneFound)
		{
			char str[256];

			sprintf_s(str, sizeof(str), "Best %f\n", Algorithm.GetBestFitnessValue());
			OutputDebugString(str);

//				DrawPairDistanceHistogram(hdc,g_dwCursorX,0,150,Algorithm.GetBest().vSamples,TCount);
		}

//		DrawDensityHistogram(hdc,g_dwCursorX,150,150,Algorithm.GetBest().vSamples,TCount);
	}

	g_dwCursorX+=200;

	for(DWORD dwI = 0; dwI < TCount; ++dwI)
		g_vBestSamplePos[dwI] = Algorithm.GetBest().vSamples[dwI];

	DrawSphereCircles(hdc,g_dwCursorX,0,150);
	DrawSamples(hdc,g_dwCursorX,0,150,Algorithm.GetBest().vSamples,TCount,100);
	DrawPairDistanceHistogram(hdc,g_dwCursorX,0,150,Algorithm.GetBest().vSamples,TCount);
	DrawDensityHistogram(hdc,g_dwCursorX,150,150,Algorithm.GetBest().vSamples,TCount);
	
	char str[256];
	sprintf_s(str, sizeof(str), "BestSampleSet %f\n", Algorithm.GetBestFitnessValue());
	TextOut(hdc,10,20,str,strlen(str));

	g_dwCursorX+=200;
}

/*
void IncrementallyFindGoodMirrorVectors( HDC hdc, vec3 *pSampleData, const DWORD dwSampleCnt, vec3 *pOutData, const DWORD dwOutCnt )
{
	vec3 vCurrentSamples[32];			assert(dwOutCnt<32);

	DrawSphereCircles(hdc,g_dwCursorX,0,150);

	for(DWORD dwMirrorI=0;dwMirrorI<dwOutCnt;++dwMirrorI)
	{
		if(dwMirrorI==0)
			vCurrentSamples[dwMirrorI]= vec3(0,0,1);
		{
			float fBestDist = -FLT_MAX;

			for(DWORD dwTry=0;dwTry<10000;++dwTry)		// try from a new random sample
			{
				vCurrentSamples[dwMirrorI] = frand_PointOnUnitSphere();

				float fMinDist = MinDistMirroredSamples(pSampleData,dwSampleCnt,vCurrentSamples,dwMirrorI,vCurrentSamples[dwMirrorI]);

				if(fMinDist>fBestDist)
				{
					fBestDist=fMinDist;

					pOutData[dwMirrorI] = vCurrentSamples[dwMirrorI];
				}
			}
		}

		DrawSamples(hdc,g_dwCursorX,0,150,&pOutData[dwMirrorI],1,100);
		DrawMirroredSamples(hdc,g_dwCursorX,450,150,pSampleData,dwSampleCnt,&pOutData[dwMirrorI],1,40);
	}
	g_dwCursorX+=200;

	DrawSphereCircles(hdc,g_dwCursorX,0,150);
	DrawSamples(hdc,g_dwCursorX,0,150,pOutData,dwOutCnt,100);
	
	g_dwCursorX+=200;

	DrawMirroredSamples(hdc,g_dwCursorX,0,150,pSampleData,dwSampleCnt,pOutData,dwOutCnt,100);
	DrawPairDistanceHistogram(hdc,g_dwCursorX,0,150,pOutData,dwOutCnt);
	DrawDensityHistogram(hdc,g_dwCursorX,150,150,pOutData,dwOutCnt);

	g_dwCursorX+=200;
}
*/

// -1..1
static float Quantize8Bit( const float x )
{
	int i = (int)((x+1.0f)*255.0f/2.0f+0.499999f);

	if(i<0)i=0;
	if(i>255)i=255;

	return (i/255.0f)*2.0f-1.0f;
}

template <int TCount>
class CGAInstance_MirrorSamples
{
public:

	void SetRandomInitalState()
	{
		for(int i = 0; i < TCount; ++i)
		{
			vSamples[i] = frand_PointOnUnitSphere();
			assert(vSamples[i].length()<1.0f/0.999f);
		}
	}

	float *GetDataAccess( DWORD &OutCount )
	{
		OutCount = TCount*3;

		return (float *)this;
	}

	void Renormalize()
	{
		for(int i = 0; i < TCount; ++i)
		{
			vSamples[i] = vSamples[i].UnsafeNormal();
/*			float Len = vSamples[i].length();

			if(Len>1.0f)
			vSamples[i] /= Len;

			float Len2 = vSamples[i].length();
			assert(Len2<1.0f/0.999f);
*/
			// 8 bit Quantize
			vSamples[i].x = Quantize8Bit(vSamples[i].x);
			vSamples[i].y = Quantize8Bit(vSamples[i].y);
			vSamples[i].z = Quantize8Bit(vSamples[i].z);
		}
	}

	void Debug()
	{
		for(int i = 0; i < TCount; ++i)
		{
			float Len = vSamples[i].length();
			assert(Len<1.0f/0.999f);
		}

		/*
		char str[256];

		for(int i = 0; i < TCount; ++i)
		{
		sprintf_s(str,sizeof(str),"(%.2f|.2%f|.2%f) ",vSamples[i].x,vSamples[i].y,vSamples[i].z);
		OutputDebugString(str);
		}
		OutputDebugString("\n");*/
	}

	// smaller means better
	float ComputeFitnessValue() const
	{
		return FitnessFuncMirroredSamples(g_vBestSamplePos,g_dwBestSamplePos,vSamples,TCount);
	}

	//private: // -------------------------------------

	vec3		vSamples[TCount];
};

template <int TCount>
void GA_FindGoodMirrorVectors( HDC hdc )
{
	TGeneticAlgorithm<CGAInstance_MirrorSamples<TCount> > Algorithm(100*POP_MULTIPLY,60*POP_MULTIPLY,0.02f);

	DrawSphereCircles(hdc,g_dwCursorX,0,150);

	for(DWORD dwI = 0; dwI < 50*TIME_MULTIPLY; ++dwI)
	{
		bool bBetterOneFound = Algorithm.Step(false);
//		DrawMirroredSamples(hdc,g_dwCursorX,0,150,g_vBestSamplePos,g_dwBestSamplePos,Algorithm.GetBest().vSamples,TCount,1);

		if(bBetterOneFound)
		{
			char str[256];

			sprintf_s(str, sizeof(str), "Best %f\n", Algorithm.GetBestFitnessValue());
			OutputDebugString(str);

			//				DrawPairDistanceHistogram(hdc,g_dwCursorX,0,150,Algorithm.GetBest().vSamples,TCount);
		}

		//		DrawDensityHistogram(hdc,g_dwCursorX,150,150,Algorithm.GetBest().vSamples,TCount);
	}

	g_dwCursorX+=200;

	for(DWORD dwI = 0; dwI < TCount; ++dwI)
		g_vBestMirrorVec[dwI] = Algorithm.GetBest().vSamples[dwI];

	DrawSphereCircles(hdc,g_dwCursorX,0,150);
	DrawMirroredSamples(hdc,g_dwCursorX,0,150,g_vBestSamplePos,g_dwBestSamplePos,Algorithm.GetBest().vSamples,TCount,100);
	DrawPairDistanceHistogram(hdc,g_dwCursorX,0,150,Algorithm.GetBest().vSamples,TCount);
	DrawDensityHistogram(hdc,g_dwCursorX,150,150,Algorithm.GetBest().vSamples,TCount);

	char str[256];
	sprintf_s(str, sizeof(str), "BestMirrorSet %f\n", Algorithm.GetBestFitnessValue());
	TextOut(hdc,10,40,str,strlen(str));

	g_dwCursorX+=200;
}


void Draw( HDC hdc )
{
	srand(13562);

	char str[256];
	sprintf_s(str, sizeof(str), "TIME_MULTIPLY:%d IN_SPHERE_POW:%f IN_SPHERE:%d POP_MULTIPLY:%d\n",TIME_MULTIPLY,(float)IN_SPHERE_POW,IN_SPHERE,POP_MULTIPLY);
	TextOut(hdc,10,0,str,strlen(str));

	{

		GA_FindGoodRandomPointsOnASphere<8>(hdc);
/*
		// twisted cube, best packing for 8 points on the sphere hull
		// http://www.research.att.com/~njas/packings/
		// http://www.enginemonitoring.com/sphere/index.htm
		g_vBestSamplePos[0] = vec3(1, 0, 0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[1] = vec3(-1, 0, 0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[2] = vec3(0, 1, 0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[3] = vec3(0, -1, 0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[4] = vec3(0.707106781186547525f, 0.707106781186547525f, -0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[5] = vec3(0.707106781186547525f, -0.707106781186547525f, -0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[6] = vec3(-0.707106781186547525f, 0.707106781186547525f, -0.594603557501360532f).UnsafeNormal();
		g_vBestSamplePos[7] = vec3(-0.707106781186547525f, -0.707106781186547525f, -0.594603557501360532f).UnsafeNormal();
*/

		{ 
			FILE *out;

			fopen_s(&out,"RandomNormal.txt","w");

			for(DWORD dwI=0;dwI<g_dwBestSamplePos;++dwI)
			{
				float fLocation = 1.0f;
#if IN_SPHERE==1
				fLocation = ComputeWeight(dwI,8);
#endif
				fprintf(out,"\tfloat3(%f, %f, %f),\n",
					g_vBestSamplePos[dwI].x*fLocation,g_vBestSamplePos[dwI].y*fLocation,g_vBestSamplePos[dwI].z*fLocation);
			}

			fclose(out);
		}

		GA_FindGoodMirrorVectors<16>(hdc);
//		IncrementallyFindGoodMirrorVectors(hdc,vSrcSamples,8,g_vBestSamples,g_dwRandomRotCnt);
//		IncrementallyFindGoodMirrorVectors(hdc,vSrcSamples,1,g_vBestSamples,g_dwRandomRotCnt);	// test
	}

/*
	{
		// twisted cube, best packing for 8 points
		// http://www.research.att.com/~njas/packings/
		// http://www.enginemonitoring.com/sphere/index.htm
		vec3 vSrcSamples[8] = 
		{
			vec3(1, 0, 0.594603557501360532f).UnsafeNormal(),
			vec3(-1, 0, 0.594603557501360532f).UnsafeNormal(),
			vec3(0, 1, 0.594603557501360532f).UnsafeNormal(),
			vec3(0, -1, 0.594603557501360532f).UnsafeNormal(),
			vec3(0.707106781186547525f, 0.707106781186547525f, -0.594603557501360532f).UnsafeNormal(),
			vec3(0.707106781186547525f, -0.707106781186547525f, -0.594603557501360532f).UnsafeNormal(),
			vec3(-0.707106781186547525f, 0.707106781186547525f, -0.594603557501360532f).UnsafeNormal(),
			vec3(-0.707106781186547525f, -0.707106781186547525f, -0.594603557501360532f).UnsafeNormal()
		};

		DrawSamples(hdc,0,0,150,vSrcSamples,8,100);
		DrawSampleHistogram(hdc,0,0,150,vSrcSamples,8);

		IncrementallyFindGoodMirrorVectors(hdc,vSrcSamples,8,g_vBestSamples,g_dwRandomRotCnt);
	}
*/
	DeInit();
}


void DeInit()
{
	std::vector<DWORD> dat;

	dat.resize(g_dwWidth*g_dwHeight);

	for(DWORD dwCY=0;dwCY<g_dwHeight;++dwCY)
		for(DWORD dwCX=0;dwCX<g_dwWidth;++dwCX)
		{
			vec3 normal = frand_PointOnUnitSphere();

			normal = g_vBestMirrorVec[(dwCX%4)+(dwCY%4)*4];

			int dwX = (int)(normal.x*127.5f+127.5f+0.4999f);		assert(dwX<0x100);
			int dwY = (int)(normal.y*127.5f+127.5f+0.4999f);		assert(dwY<0x100);
			int dwZ = (int)(normal.z*127.5f+127.5f+0.4999f);		assert(dwZ<0x100);

			dat[dwCX+dwCY*g_dwWidth] = (dwX<<16) | (dwY<<8) | dwZ;
		}

		{
			FILE *out;

			fopen_s(&out,"D:/UnrealEngine3/UTGame/Content/RandomNormal2.bmp","wb");

			BITMAPFILEHEADER fileHeader;
			fileHeader.bfType = 0x4d42;
			fileHeader.bfSize = 0;
			fileHeader.bfReserved1 = 0;
			fileHeader.bfReserved2 = 0;
			fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
			fwrite(&fileHeader,sizeof(BITMAPFILEHEADER),1,out);

			BITMAPINFOHEADER infoHeader;
			infoHeader.biSize = sizeof(infoHeader);
			infoHeader.biWidth = g_dwWidth;
			infoHeader.biHeight = g_dwHeight;
			infoHeader.biPlanes = 1;
			infoHeader.biBitCount = 32; // SPECIFY_BITS_PER_PIXEL_HERE
			infoHeader.biCompression = BI_RGB;
			infoHeader.biSizeImage = 0;
			infoHeader.biXPelsPerMeter = 0;
			infoHeader.biYPelsPerMeter = 0;
			infoHeader.biClrUsed = 0;
			infoHeader.biClrImportant = 0;
			fwrite(&infoHeader,sizeof(BITMAPINFOHEADER),1,out);

			fwrite(&dat[0],g_dwWidth*g_dwHeight*sizeof(DWORD),1,out);
			fclose(out);
		}
}

