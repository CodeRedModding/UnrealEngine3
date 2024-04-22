/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/

#pragma once
#include "BaseDevice.h"
#include "D3D9Drv.h"

const DWORD MEGABYTE = 1048576;

class VideoDevice :
	public BaseDevice
{
public:
	VideoDevice(void);
	virtual ~VideoDevice(void);
	DWORD GetPixelShaderVersion();
	DWORD GetVertexShaderVersion();
	DWORD GetVRAMQuantity();
	DWORD GetDedicatedVRAM();
	DWORD GetAdapterCount();
	bool SupportsHardwareTnL();
	bool GetnVidiaSLI();
	bool GetATICrossfire();
	void DetectVendorID();
	void DetectDeviceID();
	void DetectDriverVersion();
	void DetectDeviceName();
	void DetectVideoMemory();
	void DetectDualCards();

	void DoDeviceDetection(LPDIRECT3D9 pD3DObject);
	void DoDeviceDetection();
	static LPCWSTR GetElementName()
	{
		return L"VideoDevice";
	}


private:

	DWORD m_dwPixelShaderVersion;
	DWORD m_dwVertexShaderVersion;
	bool m_bHardwareTnL;
	DWORD m_dwVRAMQuantity;
	DWORD m_dwDedicatedVRAM;
	DWORD m_dwAdapterCount;
	bool m_fNvidiaSLI;
	bool m_fATICrossfire;
	// PCCOMPAT CHANGE -  Refcount the D3D object
	TRefCountPtr<IDirect3D9> m_pD3DObject;
	D3DADAPTER_IDENTIFIER9 m_D3DAdapterInfo;
	D3DCAPS9 m_Caps;
	
	void DetectHardwareTnL();
	void DetectPixelShaderVersion();
	void DetectVertexShaderVersion();
	HRESULT RetrieveCaps();
	HRESULT RetrieveAdapterInfo();
	void DoDxDetection();
};
