/*=============================================================================
D3D9HardwareSurvey.h: Portion of the hardware survey that uses DX to gather info.
                    Done to work around namespace pollution in UnrealEdCLR where
					the rest of the hardware survey is performed. Can't include
					d3d9.h along with all the managed namespaces which are globally
					imported.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_HARDWARESURVEYDX
#define _INC_HARDWARESURVEYDX

/** 
 * Contains the data needed by the hardware survey that is obtained from DX.
 * Can't return any DX-specific structures, so they are mostly duplicated here.
 */
struct FD3D9HardwareSurveyData
{
	FD3D9HardwareSurveyData()
	{
		appMemzero(this, sizeof(*this));
	}

    char            Driver[512];
    char            Description[512];

    QWORD			DriverVersion;          /* Defined for 32 bit components */

    DWORD           VendorId;
    DWORD           DeviceId;
    DWORD           SubSysId;
    DWORD           Revision;

    GUID            DeviceIdentifier;

	DWORD			VertexShaderVersion;
	DWORD			PixelShaderVersion;

	UINT            DesktopWidth;
	UINT            DesktopHeight;
	UINT            DesktopRefreshRate;
	UINT			DesktopBitsPerPixel;

	UINT			TotalMonitors;
	UBOOL			IsAdmin;
};

/** Function to get the DX-specific survey data from DX */
FD3D9HardwareSurveyData GetD3D9HardwareSurveyData();

#endif
