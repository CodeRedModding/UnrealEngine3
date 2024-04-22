/*=============================================================================
D3D9HardwareSurvey.cpp:
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"
#include "D3D9HardwareSurvey.h"
#include <shlobj.h>

/** Function to get the DX-specific survey data from DX */
FD3D9HardwareSurveyData GetD3D9HardwareSurveyData()
{
	FD3D9HardwareSurveyData Data;

	Data.IsAdmin = IsUserAnAdmin();

	TRefCountPtr<IDirect3D9> D3DObject;
	*D3DObject.GetInitReference() = Direct3DCreate9(D3D_SDK_VERSION);

	if (D3DObject)
	{
		// video card
		{
			D3DADAPTER_IDENTIFIER9 AdapterId;
			if (SUCCEEDED(D3DObject->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &AdapterId)))
			{
				appStrcpy(Data.Driver, AdapterId.Driver);
				appStrcpy(Data.Description, AdapterId.Description);
				Data.DriverVersion = AdapterId.DriverVersion.QuadPart;
				Data.VendorId = AdapterId.VendorId;
				Data.DeviceId = AdapterId.DeviceId;
				Data.SubSysId = AdapterId.SubSysId;
				Data.Revision = AdapterId.Revision;
				appMemcpy(&Data.DeviceIdentifier, &AdapterId.DeviceIdentifier, sizeof(AdapterId.DeviceIdentifier));
			}

			D3DCAPS9 Caps;
			if (SUCCEEDED(D3DObject->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &Caps)))
			{
				Data.VertexShaderVersion = Caps.VertexShaderVersion;
				Data.PixelShaderVersion = Caps.PixelShaderVersion;
			}
		}

		// monitor info
		{
			// D3D info
			D3DDISPLAYMODE DisplayMode;
			if (SUCCEEDED(D3DObject->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &DisplayMode)))
			{
				Data.DesktopWidth = DisplayMode.Width;
				Data.DesktopHeight = DisplayMode.Height;
				Data.DesktopRefreshRate = DisplayMode.RefreshRate;
				Data.DesktopBitsPerPixel = 
					(DisplayMode.Format == D3DFMT_A2R10G10B10 || DisplayMode.Format == D3DFMT_X8R8G8B8) ? 32 : 
					(DisplayMode.Format == D3DFMT_X1R5G5B5 || DisplayMode.Format == D3DFMT_R5G6B5) ? 16 : 0;
			}
			Data.TotalMonitors = D3DObject->GetAdapterCount();
		}
	}

	return Data;
}

VOID SetDefaultResolutionForDevice()
{
	TRefCountPtr<IDirect3D9> D3DObject;
	*D3DObject.GetInitReference() = Direct3DCreate9(D3D_SDK_VERSION);

	if (D3DObject)
	{
		// video card
		{
			D3DADAPTER_IDENTIFIER9 AdapterId;
			if (SUCCEEDED(D3DObject->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &AdapterId)))
			{
				INT DeviceCompatLevel = -1;
				FString AppCompatSection = FString::Printf(TEXT("AppCompatGPU-0x%04X"), AdapterId.VendorId);
				FString VendorName;
				UBOOL IniSectionFound = GConfig->GetString(*AppCompatSection, TEXT("VendorName"), VendorName, GSystemSettingsIni);

				// do the generic mapping first
				if (IniSectionFound)
				{
					FString DeviceIDStr = FString::Printf(TEXT("0x%04X"), AdapterId.DeviceId);
					FString DeviceInfo = GConfig->GetStr(*AppCompatSection, *DeviceIDStr, GSystemSettingsIni);

					// if we found the device, use that compat level
					if (DeviceInfo.Len() > 2)
					{
						DeviceCompatLevel = appAtoi(*DeviceInfo.Left(1));
						FString DeviceName = DeviceInfo.Right(DeviceInfo.Len()-2);
						debugf(NAME_Init, TEXT("\tGPU DeviceID found in ini: (%d) %s"), DeviceCompatLevel, *DeviceName);
					}
					else
					{
						debugf(NAME_Init, TEXT("\tGPU DeviceID not found in ini."));
					}
				}

				// We have the vendor and device ID of the GPU from above, so potentially adjust
				// the screen resolution down to accommodate older cards.
				switch (DeviceCompatLevel)
				{
					case 0:
					case 1:
					case 2:
						GConfig->SetInt(TEXT("SystemSettings"), TEXT("ResX"),  640, GSystemSettingsIni);
						GConfig->SetInt(TEXT("SystemSettings"), TEXT("ResY"),  480, GSystemSettingsIni);
						GConfig->Flush( FALSE, GEngineIni );
						break;
					case 3:
						GConfig->SetInt(TEXT("SystemSettings"), TEXT("ResX"),  800, GSystemSettingsIni);
						GConfig->SetInt(TEXT("SystemSettings"), TEXT("ResY"),  600, GSystemSettingsIni);
						GConfig->Flush( FALSE, GEngineIni );
						break;
					case 4:
					case 5:
					default:
						// No adjustment necessary
						break;
				}
			}
		}
	}
}