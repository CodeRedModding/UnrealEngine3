/*=============================================================================
	This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/

#pragma once

// PCCOMPAT CHANGE - Taken from PCCompat.h
// vvv
typedef struct _VERSION
{
	UINT MajorVersion;
	UINT MinorVersion;
	UINT RevisionVersion;
	UINT BuildVersion;
} 	Version;

typedef Version *LPVERSION;
// ^^^
// PCCOMPAT CHANGE - Taken from PCCompat.h

class BaseDevice
{
public:
	BaseDevice(void);
	virtual ~BaseDevice(void);
	static LPCWSTR GetElement() { return L"Device"; }
	DWORD GetVendorID();
	DWORD GetDeviceID();
	Version GetDriverVersion();
	LPCWSTR GetDeviceName();
	virtual void DoDeviceDetection()= 0;
	LPCWSTR GetDriverName();

protected:
	DWORD m_dwVendorID;
	DWORD m_dwDeviceID;
	static const size_t MAX_STRING_LEN = 512;
	wchar_t m_szDriverVersion[MAX_STRING_LEN];
	Version m_vDriverVersion;
	wchar_t m_szDeviceName[MAX_STRING_LEN];
	wchar_t m_szDriverName[MAX_STRING_LEN];
};
