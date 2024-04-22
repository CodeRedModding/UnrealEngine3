/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/
// PCCOMPAT CHANGE - change PCH
//#include "StdAfx.h"
#include "D3D9DrvPrivate.h"

#include "VideoDevice.h"
#include "dxdiag.h"

// PCCOMPAT CHANGE - DISABLE TIMER CODE
//#include "PCCompatTimers.h"
#define START_TIMER(index)
#define STOP_TIMER(index)

#define INITGUID
#include <initguid.h>
#include <ddraw.h>

#if _WIN64
#pragma pack(push,16)
#else
#pragma pack(push,8)
#endif
#define D3D_OVERLOADS 1
#include <d3d11.h>
#include <d3d11Shader.h>
#pragma pack(pop)

// PCCOMPAT CHANGE - DISABLE 4191 Unsafe type cast
#pragma warning(disable:4191)

namespace NvCpl
{
	#include "NvCpl.h"
}

VideoDevice::VideoDevice(void)
{
	//Initalize all the data and pointers to defaults
	m_dwVendorID = 0;
	m_dwDeviceID = 0;
	m_bHardwareTnL =false;
	m_dwPixelShaderVersion = 0;
	m_dwVertexShaderVersion = 0;
	m_dwVRAMQuantity = 0;
	m_pD3DObject = NULL;
    m_dwDedicatedVRAM = 0;
}

VideoDevice::~VideoDevice(void)
{

}

void VideoDevice::DetectVendorID()
{
	
	m_dwVendorID = m_D3DAdapterInfo.VendorId;
	
	
	//TODO:  Add fallback detection support
}

void VideoDevice::DetectDeviceID()
{
	m_dwDeviceID = m_D3DAdapterInfo.DeviceId;
	
	//TODO:  Add fallback detection support
}
void VideoDevice::DetectDriverVersion()
{
	swprintf_s(
		m_szDriverVersion, L"%d.%d.%d.%d",
		HIWORD(m_D3DAdapterInfo.DriverVersion.HighPart), 
		LOWORD(m_D3DAdapterInfo.DriverVersion.HighPart),
		HIWORD(m_D3DAdapterInfo.DriverVersion.LowPart),
		LOWORD(m_D3DAdapterInfo.DriverVersion.LowPart));
	m_vDriverVersion.MajorVersion =HIWORD(m_D3DAdapterInfo.DriverVersion.HighPart);
	m_vDriverVersion.MinorVersion =LOWORD(m_D3DAdapterInfo.DriverVersion.HighPart);
	m_vDriverVersion.RevisionVersion=HIWORD(m_D3DAdapterInfo.DriverVersion.LowPart);
	m_vDriverVersion.BuildVersion= LOWORD(m_D3DAdapterInfo.DriverVersion.LowPart);
}

void VideoDevice::DetectDeviceName()
{
	if(m_D3DAdapterInfo.Description && strlen(m_D3DAdapterInfo.Description) > 0)
	{
		if(!MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)m_D3DAdapterInfo.Description, -1, m_szDeviceName, static_cast<int>(strlen(m_D3DAdapterInfo.Description))+1))
		{
			wcscpy_s(m_szDeviceName,L"Unknown Video Card");
		}
	}
	if(m_D3DAdapterInfo.Driver && strlen(m_D3DAdapterInfo.Driver) > 0)
	{
		if(!MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)m_D3DAdapterInfo.Driver, -1, m_szDriverName, static_cast<int>(strlen(m_D3DAdapterInfo.Driver))+1))
		{
			wcscpy_s(m_szDriverName,L"Unknown Video Driver");
		}
	}
}

void VideoDevice::DetectPixelShaderVersion()
{
	//Grab the major pixel shader version
	m_dwPixelShaderVersion = D3DSHADER_VERSION_MAJOR(m_Caps.PixelShaderVersion);
	//valid shader version is between 0 and 3
	if(m_dwPixelShaderVersion < 0 || m_dwPixelShaderVersion > 3)
	{
		//Couldn't determine PS support, so assume none
		m_dwPixelShaderVersion = 0;
	}
	
}

void VideoDevice::DetectVertexShaderVersion()
{
	//Grab the major vertex shader version
	m_dwVertexShaderVersion = D3DSHADER_VERSION_MAJOR(m_Caps.VertexShaderVersion);
	//valid shader version is between 0 and 3
	// PCCOMPAT CHANGE - DISABLE < 0 check
	if(m_dwVertexShaderVersion > 3)
	{
		//Couldn't determine PS support, so assume none
		m_dwVertexShaderVersion = 0;
	}
}

void VideoDevice::DetectHardwareTnL()
{
	if(m_Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
	{
		m_bHardwareTnL = true;
	}
	else 
	{
		m_bHardwareTnL = false;
	}
}

DWORD VideoDevice::GetPixelShaderVersion()
{
	return m_dwPixelShaderVersion;
}

DWORD VideoDevice::GetVertexShaderVersion()
{
	return m_dwPixelShaderVersion;
}

bool VideoDevice::SupportsHardwareTnL()
{
	return m_bHardwareTnL;
}

DWORD VideoDevice::GetVRAMQuantity()
{
	return this->m_dwVRAMQuantity;
}

DWORD VideoDevice::GetDedicatedVRAM()
{
    return m_dwDedicatedVRAM;
}

void VideoDevice::DoDeviceDetection(LPDIRECT3D9 pD3DObject)
{
	if(pD3DObject)
	{
		//the object passed appears valid so use it to gather device info
		m_pD3DObject = pD3DObject;
	}
	DoDeviceDetection();
}

void VideoDevice::DoDeviceDetection(void)
{
#if !USE_NULL_RHI
	if(!m_pD3DObject)
	{
        START_TIMER(4);
		// PCCOMPAT CHANGE -  Refcount the D3D object
		*m_pD3DObject.GetInitReference() = Direct3DCreate9(D3D_SDK_VERSION);
        STOP_TIMER(4);
	}
	DetectDualCards();
	//Need to check the object again since it have have just been initialized by the Direct3DCreate9 call
	if(m_pD3DObject)
	{
        START_TIMER(3);
		DetectVideoMemory();
		if(SUCCEEDED(RetrieveAdapterInfo()))
		{
			//Need to have successfully obtained a D3DADAPTER_IDENTIFER struct before collecting this stuff
			DetectDeviceName();
			DetectVendorID();
			DetectDeviceID();
			DoDxDetection();
			DetectDriverVersion();
		}
		else
		{
			// PCCOMPAT CHANGE - LOG -> DEBUGF
			debugf(L"WARNING: Failed to get adapter info from the D3D Object thus couldn't get video card / driver details.");
		}
        STOP_TIMER(3);

		//Need CAPS bits to detect HW T&L and shader Version
		if(SUCCEEDED(RetrieveCaps()))
		{
			DetectHardwareTnL();
			DetectPixelShaderVersion();
			DetectVertexShaderVersion();
		}
		else
		{
			// PCCOMPAT CHANGE - LOG -> DEBUGF
			debugf(L"WARNING: Failed to get Caps bits info from the D3D Object thus couldn't get shader or HW TnL info.");
		
		}
	}
#endif //USE_NULL_RHI
}

HRESULT VideoDevice::RetrieveCaps()
{
	if(m_pD3DObject)
	{
		if(SUCCEEDED(m_pD3DObject->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &m_Caps)))
		{
			return S_OK;
		}
	}
	
	return E_FAIL;
}

HRESULT VideoDevice::RetrieveAdapterInfo()
{
	if(m_pD3DObject)
	{
        m_dwAdapterCount = m_pD3DObject->GetAdapterCount();
        
		if(SUCCEEDED(m_pD3DObject->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &m_D3DAdapterInfo)))
		{
			return S_OK;
		}
	}
	
	return E_FAIL;
	
}
void VideoDevice::DoDxDetection()
{
}

void VideoDevice::DetectVideoMemory()
{
	IDirectDraw7* DDobject;
	HINSTANCE ddrawLib = LoadLibrary( L"ddraw.dll" );
	if (!ddrawLib)
	{
		debugf(L"WARNING: Couldn't find DirectDraw7.dll, so we can't calculate the amount of Video Ram");
		return;
	}

	HRESULT (WINAPI* _DirectDrawCreateEx)( GUID* lpGUID, void** lplpDD, REFIID iid, IUnknown* pUnkOuter ) = 
		(HRESULT (WINAPI*)( GUID* lpGUID, void** lplpDD, REFIID iid, IUnknown* pUnkOuter ))GetProcAddress( ddrawLib, "DirectDrawCreateEx" ); 
	
	if( !_DirectDrawCreateEx  )
	{
		debugf(L"WARNING: Failed to load DirectDrawCreateEx() function from DirectDraw7.dll, so we can't calculate the amount of Video Ram");
		return;
	}

	HRESULT (WINAPI* _DirectDrawEnumerateEx)( LPDDENUMCALLBACKEX, LPVOID, DWORD ) = 
		(HRESULT (WINAPI*)( LPDDENUMCALLBACKEX, LPVOID, DWORD )) GetProcAddress( ddrawLib, "DirectDrawEnumerateExA" ); 
	if( !_DirectDrawEnumerateEx  )
	{
		debugf(L"WARNING: Failed to load DirectDrawEnumerateEx() from DirectDraw7.dll, so we can't calculate the amount of Video Ram");
		return;
	}

	
	HRESULT result=_DirectDrawCreateEx( NULL, (void**)&DDobject, IID_IDirectDraw7, NULL );
	if( FAILED(result) )
	{
		debugf(L"WARNING: Failed to calculate the amount of Video Ram");
		return;
	}

	DDobject->SetCooperativeLevel( NULL, DDSCL_NORMAL );
	//
	// See how much memory is on the card
	//
	DDSCAPS2 ddscaps2;
	DWORD Junk;
	DWORD VideoMemory=0xffffffff;	// otherwise known as -1
	DWORD Temp=0;
	memset( &ddscaps2, 0, sizeof(ddscaps2) );
	ddscaps2.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
	DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
	if( Temp>0 && Temp<VideoMemory )
		VideoMemory=Temp;
	ddscaps2.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_LOCALVIDMEM | DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
	DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
	if( Temp>0 && Temp<VideoMemory )
		VideoMemory=Temp;
	ddscaps2.dwCaps = DDSCAPS_LOCALVIDMEM | DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
	DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
	if( Temp>0 && Temp<VideoMemory )
		VideoMemory=Temp;
	ddscaps2.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_LOCALVIDMEM | DDSCAPS_VIDEOMEMORY;
	DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
	if( Temp>0 && Temp<VideoMemory )
		VideoMemory=Temp;
	//
	// If cannot get memory size, there must be a problem with D3D / DirectDraw
	//
	if( -1 == VideoMemory )
	{
		debugf(L"WARNING: Couldn't calculate the amount of Video Ram");
		m_dwVRAMQuantity = 0;
		return;
	}

	//
	// Round the video memory number depending upon the size
	//
	if( VideoMemory <= 16 * MEGABYTE )
	{
		VideoMemory=((VideoMemory + 8 * MEGABYTE - 1) & ~(8 * MEGABYTE - 1));				// Round up to nearest 8 under 20Meg
	}
	else
	{
		if( VideoMemory <= 64 * MEGABYTE )
		{
			VideoMemory=((VideoMemory + 32 * MEGABYTE - 1) & ~(32 * MEGABYTE - 1));			// Round to neaest 32 Meg under 64Meg
		}
		else
		{
			VideoMemory=((VideoMemory + 64 * MEGABYTE - 1) & ~(64 * MEGABYTE - 1));			// Round to neaest 64 Meg over 64Meg
		}
	}

    LPDIRECTDRAW2 pdd2;
    if (SUCCEEDED(DDobject->QueryInterface(IID_IDirectDraw2, (void**)&pdd2)))
    {
        DDSCAPS ddscaps;
        ddscaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
        pdd2->GetAvailableVidMem(&ddscaps, &Temp, NULL);
        pdd2->Release();
    }

    m_dwDedicatedVRAM = Temp >> 20;
    
	//
	// Clean up ddraw7
	//
	m_dwVRAMQuantity=(VideoMemory >> 20);
	DDobject->Release();
	DDobject = NULL;
	FreeLibrary( ddrawLib );
}

DWORD VideoDevice::GetAdapterCount()
{
    return m_dwAdapterCount;
}

typedef INT (*ATIQUERYMGPUCOUNT)();

INT AtiMultiGPUAdapters()
{
	HINSTANCE lib = LoadLibrary(TEXT("ATIMGPUD.DLL"));
	if (!lib)
		return 1;

	ATIQUERYMGPUCOUNT AtiQueryMgpuCount;
	AtiQueryMgpuCount = (ATIQUERYMGPUCOUNT)GetProcAddress(lib, "AtiQueryMgpuCount");
	if (!AtiQueryMgpuCount)
		return 1;

	INT count = AtiQueryMgpuCount();
	if (count < 1) count = 1;

	FreeLibrary(lib);

	return count;
}

void VideoDevice::DetectDualCards()
{
    m_fNvidiaSLI = false;
    m_fATICrossfire = false;
    
    // First do nVidia
    HINSTANCE hLib = LoadLibrary(L"NVCPL.DLL");
    if (hLib)
    {
		NvCpl::NvCplGetDataIntType NvCplGetDataInt = (NvCpl::NvCplGetDataIntType)::GetProcAddress(hLib, "NvCplGetDataInt");
        if (NvCplGetDataInt)
        {
            long lSLIGPUs = 0;
            if (NvCplGetDataInt(NVCPL_API_NUMBER_OF_SLI_GPUS, &lSLIGPUs) && lSLIGPUs > 0)
            {
                long lSLIMode = 0;
                if (NvCplGetDataInt(NVCPL_API_SLI_MULTI_GPU_RENDERING_MODE, &lSLIMode) && (lSLIMode & NVCPL_API_SLI_ENABLED))
                {
                    m_fNvidiaSLI = true;
                }
            }
        }
        FreeLibrary(hLib);
    }

    if (AtiMultiGPUAdapters() > 1)
    {
        m_fATICrossfire = true;
    }
}

bool VideoDevice::GetnVidiaSLI()
{
    return m_fNvidiaSLI;
}

bool VideoDevice::GetATICrossfire()
{
    return m_fATICrossfire;
}
