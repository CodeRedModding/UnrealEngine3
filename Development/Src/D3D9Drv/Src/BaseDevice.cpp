/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/
// PCCOMPAT CHANGE - change PCH
//#include "stdafx.h"
#include "D3D9DrvPrivate.h"

#include "BaseDevice.h"

BaseDevice::BaseDevice(void)
{
	m_dwVendorID = 0;
	m_dwDeviceID = 0;
	ZeroMemory((void *)&m_vDriverVersion, sizeof(m_vDriverVersion));
	ZeroMemory((void *)m_szDriverVersion,sizeof(m_szDriverVersion));
	ZeroMemory((void *)m_szDeviceName,sizeof(m_szDeviceName));
	ZeroMemory((void *)m_szDriverName,sizeof(m_szDriverName));
}

BaseDevice::~BaseDevice(void)
{
}

DWORD BaseDevice::GetDeviceID()
{
	return m_dwDeviceID;
}

Version BaseDevice::GetDriverVersion()
{
	return m_vDriverVersion;
}
LPCWSTR BaseDevice::GetDeviceName()
{
	return m_szDeviceName;
}
LPCWSTR BaseDevice::GetDriverName()
{
	return m_szDriverName;
}
DWORD BaseDevice::GetVendorID()
{
	return m_dwVendorID;
}
