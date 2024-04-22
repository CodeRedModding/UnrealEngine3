/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "ManagedCodeSupportCLR.h"
#include "HardwareSurveyShared.h"
#include "D3D9HardwareSurvey.h"

using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Management;
using namespace System::Text;
using namespace System::Xml;
using namespace System::Xml::Serialization;
using namespace Microsoft::Win32;

/** 
* Namespace wrapping the classes used to store the hardware survey items.
* XML-friendly class to serialize in a reasonably compact form.
*/
namespace UDKSurveyPostProcessor
{
	public ref class UDKHeader
	{
	public:
		[XmlAttribute]
		INT Version;

		[XmlAttribute]
		String^ ModName;

		[XmlAttribute]
		Guid ModGuid;

		[XmlAttribute]
		Guid ModAuthorID;

		[XmlAttribute]
		Guid ID;

		[XmlAttribute]
		INT SurveyTimeMS;

		[XmlAttribute]
		INT SurveysAttempted;
		
		[XmlAttribute]
		INT SurveysFailed;
	};

	public ref class Locale
	{
	public:
		[XmlAttribute]
		String^ Culture;

		[XmlAttribute]
		String^ TimeZone;
	};

	public ref class OperatingSystem
	{
	public:
		[XmlAttribute]
		String^ Platform;

		[XmlAttribute]
		String^ Version;

		[XmlAttribute]
		String^ ServicePack;

		[XmlAttribute]
		WORD ServicePackMajor;

		[XmlAttribute]
		WORD ServicePackMinor;

		[XmlAttribute]
		WORD SuiteMask;

		[XmlAttribute]
		BYTE ProductType;

		[XmlAttribute]
		String ^Caption;

		[XmlAttribute]
		bool Is64Bit;

		/// <summary>
		/// valid values: null, true, false, N/A, NotFound, Exception
		/// </summary>
		[XmlAttribute]
		String^ IsUACEnabled;

		[XmlAttribute]
		bool IsAdmin;
	};

	public ref class Processors
	{
	public:
		[XmlAttribute]
		String^ Description;

		[XmlAttribute]
		USHORT Family;

		[XmlAttribute]
		String^ Manufacturer;

		[XmlAttribute]
		UINT MaxClockSpeed;

		[XmlAttribute]
		USHORT DataWidth;

		[XmlAttribute]
		String^ Name;

		[XmlAttribute]
		UINT NumberOfPhysicalProcessors;

		[XmlAttribute]
		UINT NumberOfCores;

		[XmlAttribute]
		UINT NumberOfLogicalProcessors;

		[XmlAttribute]
		String^ Features;
	};

	public ref class VideoCard
	{
	public:
		[XmlAttribute]
		String^ Driver;

		[XmlAttribute]
		String^ Description;

		[XmlAttribute]
		QWORD DriverVersion;

		[XmlAttribute]
		UINT VendorID;

		[XmlAttribute]
		UINT DeviceID;

		[XmlAttribute]
		UINT SubSysID;

		[XmlAttribute]
		UINT Revision;

		[XmlAttribute]
		Guid DeviceIdentifier;

		[XmlAttribute]
		USHORT PixelShaderVersionDX9;

		[XmlAttribute]
		USHORT VertexShaderVersionDX9;

		[XmlAttribute]
		UINT TotalVRAM;
	};

	public ref class Monitor
	{
	public:
		[XmlAttribute]
		UINT Width;

		[XmlAttribute]
		UINT Height;

		[XmlAttribute]
		UINT BitsPerPixel;

		[XmlAttribute]
		UINT RefreshRate;

		[XmlAttribute]
		UINT VirtualScreenWidth;

		[XmlAttribute]
		UINT VirtualScreenHeight;

		[XmlAttribute]
		BYTE MaxHorizontalImageSizeCm;

		[XmlAttribute]
		BYTE MaxVerticalImageSizeCm;
	};


	public ref class Hardware
	{
	public:
		Hardware()
		{
			ProcessorInfo = gcnew Processors();
			PrimaryVideoCard = gcnew VideoCard();
			PrimaryMonitor = gcnew Monitor();
			AppCompatLevelCPU = -1;
			AppCompatLevelGPU = -1;
			AppCompatLevelComposite = -1;
		}

		[XmlAttribute]
		QWORD TotalPhysicalMemory;

		[XmlAttribute]
		QWORD TotalHardDriveSize;

		[XmlAttribute]
		QWORD TotalHardDriveFreeSpace;

		[XmlElement("Processors")]
		Processors^ ProcessorInfo;

		VideoCard^ PrimaryVideoCard;

		Monitor^ PrimaryMonitor;

		[XmlAttribute]
		UINT TotalMonitors;

		[XmlAttribute]
		UINT TotalVideoCards;

		[XmlAttribute]
		System::SByte AppCompatLevelCPU;

		[XmlAttribute]
		System::SByte AppCompatLevelGPU;

		[XmlAttribute]
		System::SByte AppCompatLevelComposite;
	};

	/// <summary>
	/// Data structure defining the data that will be processed from the raw data in XML format. Uses XmlSerializer.
	/// </summary>
	[XmlRoot("Survey")]
	public ref class UDKSurvey
	{
	public: 
		UDKHeader^ UDKHeader;

		Locale^ Locale;

		OperatingSystem^ OperatingSystem;

		Hardware^ Hardware;

		/// <summary>
		/// If an untrapped exception occurs during collection, the results are
		/// placed in this string so they will be available for diagnosis at the data center.
		/// </summary>
		String^ DataCollectionErrorString;

		UDKSurvey()
		{
			UDKHeader = gcnew UDKSurveyPostProcessor::UDKHeader();
			OperatingSystem = gcnew UDKSurveyPostProcessor::OperatingSystem();
			Locale = gcnew UDKSurveyPostProcessor::Locale();
			Hardware = gcnew UDKSurveyPostProcessor::Hardware();
			DataCollectionErrorString = String::Empty;
		}

	};


	/** This is a fragment of the class that contains the mod options. Just need it to load the basic mod info. */
	[XmlRoot]
	public ref class GameOptions
	{
	public:
		[XmlAttribute]
		String^ GameName;

		[XmlElement]
		Guid GameUniqueID;

		[XmlElement]
		Guid MachineUniqueID;
	};

}

/** 
* Wrapper around getting a WMI object. Traps exceptions for values that aren't there
* and returns a default value.
*/
template<typename Type>
static Type GetWMIProperty(ManagementObject^ Obj, String^ Property, String^% ErrStr)
{
	// eat exceptions. If this fails, return default value and carry on.
	try
	{
		Object^ Prop = Obj->GetPropertyValue(Property);
		if (Prop)
		{
			return (Type)Prop;
		}
		else if (ErrStr)
		{
			ErrStr += "WMI property '" + Property + "' not found. " + "\n\n";
		}
	}
	catch(Exception^ Ex)
	{
		if (ErrStr)
		{
			ErrStr += "WMI property '" + Property + "' threw exception. " + Ex->ToString() + "\n\n";
		}
	}
	return Type();
}

/** Quick way to parse a native GUID into a managed GUID */
Guid FromGUID( GUID& SrcGuid ) 
{
	return Guid( 
		SrcGuid.Data1, SrcGuid.Data2, SrcGuid.Data3, 
		SrcGuid.Data4[0], SrcGuid.Data4[1], SrcGuid.Data4[2], SrcGuid.Data4[3], 
		SrcGuid.Data4[4], SrcGuid.Data4[5], SrcGuid.Data4[6], SrcGuid.Data4[7] );
}


/** 
* Gets or creates the unique key for a machine used for the UDK survey.
* Exceptions are eaten so we don't crash, and a zero guid is returned.
*/
static Guid GetUDKGeneratedID()
{
	String^ UDKKeyStr = "Software\\Epic Games\\UDK";
	String^ IDKeyStr = "ID";
	String^ IDCKKeyStr = "IDCK";
	Guid IDGuid;
	bool bFoundGuid = false;

	try
	{
		RegistryKey^ UDKKey = Registry::CurrentUser->CreateSubKey(UDKKeyStr);
		Object^ IDValue = UDKKey->GetValue(IDKeyStr);

		if (IDValue != nullptr)
		{
			// ID already exists, so parse it
			try
			{
				IDGuid = Guid(IDValue->ToString());
				bFoundGuid = true;
			}
			catch (FormatException^)
			{
				// ID was not a Guid
			}
		}

		// get the MAC address for verification purposes.
		// some installer wrappers blankly copy all regkeys on install, thus creating
		// duplicate keys for values that should be globally unique (like GUIDs).
		// Therefore, when we run, if the MACID doesn't match the actual MAC,
		// we assume someone has copied the registry and we regenerate.
		// if there is no MACID, we just use the default zero.
		// If someone changes their network card this will effectively change the machine
		// as far as the hardware survey is concerned.w
		Int64 MACID;
		DWORD MACIDLen = sizeof(MACID);
		appMemzero(&MACID, MACIDLen);
		appGetMacAddress((BYTE*)&MACID, MACIDLen);

		// if the ID does exist, verify it against the MACID.
		if (bFoundGuid)
		{
			Object^ IDCKValueRaw = UDKKey->GetValue(IDCKKeyStr);
			// if we can't parse the MACID, treat it like it's incorrect and regenerate it.
			if (IDCKValueRaw != nullptr)
			{
				try
				{
					Int64 IDCKValue = (Int64)IDCKValueRaw;
					if (MACID != IDCKValue)
					{
						// the MACIDs didn't match. regenerate.
						bFoundGuid = false;
					}
				}
				catch (Exception^)
				{
					// we couldn't parse the MACID key.regenerate.
					bFoundGuid = false;
				}
			}
			else
			{
				// MACID key not found.regenerate.
				bFoundGuid = false;
			}
		}

		// if the ID didn't exist or we couldn't parse it, then create it
		if (!bFoundGuid)
		{
			IDGuid = Guid::NewGuid();
			UDKKey->SetValue(IDKeyStr, IDGuid.ToString());
			UDKKey->SetValue(IDCKKeyStr, MACID, RegistryValueKind::QWord);
		}
		return IDGuid;
	}
	catch (Exception^)
	{
		// we just need to silently deal with this case, which should never happen
		return Guid::Empty;
	}
}

#pragma managed(push, off)
/** unmanaged wrapper around call to __cpuid. Can't be called from C++/CLI code directly. */
static void CallCPUID(INT CPUInfo[4], INT InfoType)
{
	__cpuid(CPUInfo, InfoType);
}
#pragma managed(pop)

/** 
* Uses CPUID to determine the features of the CPU.
* Returns the 4 ints from InfoType == 1 as csv strings for further downstream analysis at the datacenter.
* Parses some common values and returns them as additional csv:
* SSE, SSE2, SSE3, SSE4.1, SSE4.2, HTT (hyperthreading)
*/
static String^ GetCPUFeatures()
{
	String^ Result = String::Empty;
	try
	{
		INT CPUInfo[4];
		CallCPUID(CPUInfo, 0);
		INT MaxInfoType = CPUInfo[0];
		if (MaxInfoType > 0)
		{
			CallCPUID(CPUInfo, 1);
			Result += String::Format("{0:X8},{1:X8},{2:X8},{3:X8},", CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
			Result += (CPUInfo[3] & (1 << 25)) != 0 ? "SSE," : "";
			Result += (CPUInfo[3] & (1 << 26)) != 0 ? "SSE2," : "";
			Result += (CPUInfo[2] & (1 <<  0)) != 0 ? "SSE3," : "";
			Result += (CPUInfo[2] & (1 << 19)) != 0 ? "SSE4.1," : "";
			Result += (CPUInfo[2] & (1 << 20)) != 0 ? "SSE4.2," : "";
			Result += (CPUInfo[3] & (1 << 23)) != 0 ? "MMX," : "";
			Result += (CPUInfo[3] & (1 << 28)) != 0 ? "HTT," : "";
		}
	}
	catch(...)
	{
		// just eat problems and don't report CPU features.
	}
	return Result;
}

/** 
* returns the windows device id of the desktop monitor 
* Helper used to determine the physical dimensions of the monitor.
*/
static String^ GetDeviceIDOfDesktopMonitor()
{
	// marshal the primary monitor device name into native code.
	String^ PrimaryDeviceNameManaged = System::Windows::Forms::Screen::PrimaryScreen->DeviceName;
	FString PrimaryDeviceName = CLRTools::ToFString(PrimaryDeviceNameManaged);
	// call EnumDisplayDevices to ge the DeviceID.
	DISPLAY_DEVICE PrimaryMonitorDevice;
	appMemzero(&PrimaryMonitorDevice, sizeof(PrimaryMonitorDevice));
	PrimaryMonitorDevice.cb = sizeof(PrimaryMonitorDevice);
	if (!EnumDisplayDevices(*PrimaryDeviceName, 0, &PrimaryMonitorDevice, 0)) 
		throw gcnew ApplicationException(String::Format("EnumDisplayDevices failed for DeviceName {0}.", PrimaryDeviceNameManaged));
	return gcnew String(PrimaryMonitorDevice.DeviceID);
}

/**
 * try to retrieve the (approximate) physical dimensions of the primary monitor.
 * Use the EDID reported by the primary desktop monitor, which isn't techincally the true size, but
 * works quite well in practice (except for projectors and other odd display devices).
 *
 * Vista supports WmiMonitorBasicDisplayParams, but gives no easy way to relate to which monitor is which on the system.
 * Extensive tests show that the enumeration order is not predictable. 
 * WmiMonitorBasicDisplayParams provides an InstanceName that is the Plug-n-Play ID of the monitor.
 * System.Windows.Forms.Screen.PrimaryScreen can return the DeviceName of the primary monitor.
 * EnumDisplayDevices can be used to return the DeviceID of that monitor.
 * By scouring the registry, one can find the monitor whose the DriverName value matches the DeviceID from above.
 * That registry key is named for the PNP ID of the monitor.
 * Then WMI can be used on Vista to determine the physical size of the monitor.
 * On pre-Vista, the EDID can be read directly from the registry and used to exact the same information.
 * Since it works the same either way, just always read the EDID.
 */
static void RecordPhysicalDimensionsOfDesktopMonitor(UDKSurveyPostProcessor::UDKSurvey^ SurveyData)
{
	// don't want to display errors if we eventually found the dimensions. But if we didn't, make sure 
	// we report errors along the way.
	String^ NonFatalErrors = String::Empty;
	String^ DesktopMonitorDeviceID = GetDeviceIDOfDesktopMonitor();
	// split the device ID up.
	// Empirically tested, it is of the form MONITOR\{ManufacturerID}\{GUID}\{monitor instance}
	array<String^>^ DeviceIDParts = DesktopMonitorDeviceID->Split(gcnew array<System::Char> {'\\'}, 3);
	if (DeviceIDParts->Length != 3) throw gcnew ApplicationException(String::Format("DesktopMonitorDeviceID {0} does not have 3 parts", DesktopMonitorDeviceID));
	
	// now search for that monitor in the registry, which has the form {ManufacturerID}\PNPID\[DriverName=DeviceID]
	RegistryKey^ RootDisplayRegKey = Registry::LocalMachine->OpenSubKey("SYSTEM\\CurrentControlSet\\Enum\\Display");
	if (RootDisplayRegKey == nullptr) throw gcnew ApplicationException("Failed to open RegKey HKLM\\SYSTEM\\CurrentControlSet\\Enum\\Display");
	array<String^>^ RegKeysToSearch = RootDisplayRegKey->GetSubKeyNames();
	for each (String^ SearchKey in RegKeysToSearch)
	{
		// If the registry key is "Default_Monitor", just skip it--it doesn't appear to contain a valid EDID key (we normally see BAD_EDID entries)
		if (SearchKey->Equals("Default_Monitor", StringComparison::InvariantCultureIgnoreCase)) continue;

		// no need to check result here. we already know it exists.
		RegistryKey^ RegKey = RootDisplayRegKey->OpenSubKey(SearchKey);
		// iterate over all the devices in this regkey, looking for the one with a Driver value equal to the one we are looking for
		for each (String^ SubKeyName in RegKey->GetSubKeyNames())
		{
			// no need to check result here. we already know it exists.
			RegistryKey^ SubKey = RegKey->OpenSubKey(SubKeyName);
			String^ DriverName = (String^)SubKey->GetValue("Driver");
			if (DriverName == nullptr) 
			{
				// we can keep searching for other displays in this case, so don't throw an exception.
				NonFatalErrors += String::Format("Driver value not found in SubKey {0}.\n\n", SubKey->Name);
				continue;
			}

			// if we found the correct Driver name, open the EDID key and read the fields that we want.
			if (DeviceIDParts[2]->Equals(DriverName, StringComparison::InvariantCultureIgnoreCase))
			{
				RegistryKey^ EDIDKey = SubKey->OpenSubKey("Device Parameters");
				if (EDIDKey == nullptr) 
				{
					NonFatalErrors += String::Format("Device Parameters SubKey not found in {0}.\n\n", SubKey->Name);
					continue;
				}
				array<System::Byte>^ EDIDValue = (array<System::Byte>^)EDIDKey->GetValue("EDID");
				if (EDIDValue == nullptr)
				{
					NonFatalErrors += String::Format("EDID value not found in {0}.\n\n", EDIDKey->Name);
					continue;
				}
				// The EDID is a standard byte array. Bytes 21 and 22 are the physical size of the monitor in Cm.
				// Well, technically it is not quite true, but for most standard monitors it is.
				// Search the web for EDID references.
				System::Byte WidthCm = EDIDValue[21];
				System::Byte HeightCm = EDIDValue[22];
				SurveyData->Hardware->PrimaryMonitor->MaxHorizontalImageSizeCm = WidthCm;
				SurveyData->Hardware->PrimaryMonitor->MaxVerticalImageSizeCm = HeightCm;
				return;
			}
		}
	}
	if (!String::IsNullOrEmpty(NonFatalErrors))
	{
		NonFatalErrors = "Errors found while searching: " + NonFatalErrors;
	}

	throw gcnew ApplicationException(String::Format("Could not find matching PNPID for Desktop monitor DeviceID {0}. {1}", DesktopMonitorDeviceID, NonFatalErrors));
}


/** Creates the structure containing the hardware survey information and serializes it to XML */
static UDKSurveyPostProcessor::UDKSurvey^ RetrieveHardwareInfo()
{
	using namespace UDKSurveyPostProcessor;

	// This will contain all the hardware info in an XMl-serializable struct.
	UDKSurvey^ SurveyData = gcnew UDKSurvey();

	try
	{
		DateTime SurveyStart = DateTime::UtcNow;

		// WMI searches use this searcher.
		ManagementObjectSearcher^ Searcher = gcnew ManagementObjectSearcher();

		// Header info
		{
			SurveyData->UDKHeader->Version = GEngineVersion;
			SurveyData->UDKHeader->ModName = "UDK";
			SurveyData->UDKHeader->ModGuid = Guid::Empty;
			SurveyData->UDKHeader->ModAuthorID = Guid::Empty;
			try
			{
				// we know we are a mod if we don't have a Content directory
				bool bIsUDKInstall = Directory::Exists(IO::Path::Combine(gcnew String(*appGameDir()), "Content"));
				if (!bIsUDKInstall)
				{
					// clear out the modname so on error, we will know it wasn't a UDK install.
					SurveyData->UDKHeader->ModName = "";

					// if we are not UDK, grab the mod info and report it.
					// The mod info file should be there else it's an error
					UDKSurveyPostProcessor::GameOptions^ Opts = (GameOptions^)(gcnew XmlSerializer(UDKSurveyPostProcessor::GameOptions::typeid))->Deserialize(File::OpenText(gcnew String(appBaseDir()) + "..\\..\\Binaries\\UnSetup.Game.xml"));
					SurveyData->UDKHeader->ModName = Opts->GameName;
					SurveyData->UDKHeader->ModGuid = Opts->GameUniqueID;
					SurveyData->UDKHeader->ModAuthorID = Opts->MachineUniqueID;

				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to load UnSetup.Game.xml file that should be present. " + Ex->ToString() + "\n\n";
			}
			SurveyData->UDKHeader->ID = GetUDKGeneratedID();
		}
		//debugf(TEXT("01 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		// Locale info
		{
			SurveyData->Locale->Culture = System::Globalization::CultureInfo::CurrentCulture->Name;
			SurveyData->Locale->TimeZone = TimeZone::CurrentTimeZone->StandardName;
		}
		//debugf(TEXT("02 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		// OS info
		{
			// Using .NET
			SurveyData->OperatingSystem->Platform = Environment::OSVersion->Platform.ToString();
			SurveyData->OperatingSystem->ServicePack = Environment::OSVersion->ServicePack;
			SurveyData->OperatingSystem->Version = Environment::OSVersion->Version->ToString();

			// Check UAC
			// only supported on Vista
			if (Environment::OSVersion->Version->Major >= 6)
			{
				try
				{
					Microsoft::Win32::RegistryKey^ Key = Microsoft::Win32::Registry::LocalMachine->OpenSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
					if (Key != nullptr)
					{
						Object^ Value = Key->GetValue("EnableLUA");
						if (Value != nullptr)
						{
							SurveyData->OperatingSystem->IsUACEnabled = (INT)Value != 0 ? "true" : "false";
						}
						else
						{
							SurveyData->OperatingSystem->IsUACEnabled = "NotFound";
						}
					}
					else
					{
						SurveyData->OperatingSystem->IsUACEnabled = "NotFound";
					}
				}
				catch(Exception^ Ex)
				{
					SurveyData->OperatingSystem->IsUACEnabled = "Exception";
					SurveyData->DataCollectionErrorString += "Failed to check UAC settings. " + Ex->ToString() + "\n\n";
				}
			}
			else
			{
				SurveyData->OperatingSystem->IsUACEnabled = "N/A";
			}
			//debugf(TEXT("03 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

			// Using Win32
			OSVERSIONINFOEX OsVersionInfo;
			ZeroMemory( &OsVersionInfo, sizeof( OsVersionInfo ) );
			OsVersionInfo.dwOSVersionInfoSize = sizeof( OsVersionInfo );
			GetVersionEx( (LPOSVERSIONINFO)&OsVersionInfo );
			SurveyData->OperatingSystem->ServicePackMajor = OsVersionInfo.wServicePackMajor;
			SurveyData->OperatingSystem->ServicePackMinor = OsVersionInfo.wServicePackMinor;
			SurveyData->OperatingSystem->SuiteMask = OsVersionInfo.wSuiteMask;
			SurveyData->OperatingSystem->ProductType = OsVersionInfo.wProductType;
			//debugf(TEXT("04 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

			// Using WMI
			try
			{
				Searcher->Query = gcnew ObjectQuery( "Select Caption from Win32_OperatingSystem" );
				for each ( ManagementObject^ Object in Searcher->Get() )
				{
					SurveyData->OperatingSystem->Caption = GetWMIProperty<String^>(Object, "Caption", SurveyData->DataCollectionErrorString);
					break;
				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to get Win32_OperatingSystem WMI entry. " + Ex->ToString() + "\n\n";
			}
			//debugf(TEXT("05 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

			// Using UE3
			SurveyData->OperatingSystem->Is64Bit = appIs64bitOperatingSystem() != 0;
			//debugf(TEXT("06 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);
		}

		// Memory
		{
			try
			{
				// WMI info
				Searcher->Query = gcnew ObjectQuery( "Select Capacity from Win32_PhysicalMemory" );
				for each ( ManagementObject^ Object in Searcher->Get() )
				{
					SurveyData->Hardware->TotalPhysicalMemory += GetWMIProperty<QWORD>(Object, "Capacity", SurveyData->DataCollectionErrorString);
				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to get Win32_PhysicalMemory WMI entry. " + Ex->ToString() + "\n\n";
			}
		}
		//debugf(TEXT("07 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		// Hard disks
		{
			try
			{
				Searcher->Query = gcnew ObjectQuery( "Select Size, FreeSpace from Win32_LogicalDisk WHERE MediaType = 12" );
				for each ( ManagementObject^ Object in Searcher->Get() )
				{
					SurveyData->Hardware->TotalHardDriveSize += GetWMIProperty<QWORD>(Object, "Size", SurveyData->DataCollectionErrorString);
					SurveyData->Hardware->TotalHardDriveFreeSpace += GetWMIProperty<QWORD>(Object, "FreeSpace", SurveyData->DataCollectionErrorString);
				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to get Win32_LogicalDisk WMI entry. " + Ex->ToString() + "\n\n";
			}
		}
		//debugf(TEXT("08 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		// Processors
		{
			try
			{
				// WMI info
				Searcher->Query = gcnew ObjectQuery( "Select * from Win32_Processor" );
				for each ( ManagementObject^ Object in Searcher->Get() )
				{
					// grab general info from first processor
					if (SurveyData->Hardware->ProcessorInfo->NumberOfPhysicalProcessors == 0)
					{
						SurveyData->Hardware->ProcessorInfo->Description = GetWMIProperty<String^>(Object, "Description", SurveyData->DataCollectionErrorString);
						SurveyData->Hardware->ProcessorInfo->Family = GetWMIProperty<USHORT>(Object, "Family", SurveyData->DataCollectionErrorString);
						SurveyData->Hardware->ProcessorInfo->Manufacturer = GetWMIProperty<String^>(Object, "Manufacturer", SurveyData->DataCollectionErrorString);
						SurveyData->Hardware->ProcessorInfo->MaxClockSpeed = GetWMIProperty<UINT>(Object, "MaxClockSpeed", SurveyData->DataCollectionErrorString);
						SurveyData->Hardware->ProcessorInfo->DataWidth = GetWMIProperty<USHORT>(Object, "DataWidth", SurveyData->DataCollectionErrorString);
						SurveyData->Hardware->ProcessorInfo->Name = GetWMIProperty<String^>(Object, "Name", SurveyData->DataCollectionErrorString);
						SurveyData->Hardware->ProcessorInfo->Features = GetCPUFeatures();
					}

					SurveyData->Hardware->ProcessorInfo->NumberOfPhysicalProcessors++;
					// These values are known to fail on pre WinXP SP2 and Server 2k3, so don't record exceptions for them.
					String^ DummyErrStr = nullptr;
					SurveyData->Hardware->ProcessorInfo->NumberOfCores += GetWMIProperty<UINT>(Object, "NumberOfCores", DummyErrStr);
					SurveyData->Hardware->ProcessorInfo->NumberOfLogicalProcessors += GetWMIProperty<UINT>(Object, "NumberOfLogicalProcessors", DummyErrStr);
				}

				// WinXP SP3 and certain versions of Server 2k3 do not support NumberOfCores or NumberOfLogicalProcessors.
				// http://support.microsoft.com/kb/936235
				// http://support.microsoft.com/default.aspx/kb/932370/
				// In those cases, we will get zero values for NumberOfCores and NumberOfLogicalProcessors. 
				// Check for that and use alternate logic to get number of logical processors
				if (SurveyData->Hardware->ProcessorInfo->NumberOfCores == 0)
				{
					// in this case, the old behavior is to report an instance for each logical processor, so do that and report -1 for everything else.
					SurveyData->Hardware->ProcessorInfo->NumberOfLogicalProcessors = SurveyData->Hardware->ProcessorInfo->NumberOfPhysicalProcessors;
					SurveyData->Hardware->ProcessorInfo->NumberOfPhysicalProcessors = 0;
					SurveyData->Hardware->ProcessorInfo->NumberOfCores = 0;
				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to get Win32_Processor WMI entry. " + Ex->ToString() + "\n\n";
			}
		}
		//debugf(TEXT("09 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);


		// hardware/monitor info
		{
			FD3D9HardwareSurveyData D3D9Data = GetD3D9HardwareSurveyData();
			//debugf(TEXT("10 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);
			SurveyData->Hardware->PrimaryVideoCard->Driver = gcnew String(D3D9Data.Driver);
			SurveyData->Hardware->PrimaryVideoCard->Description = gcnew String(D3D9Data.Description);
			SurveyData->Hardware->PrimaryVideoCard->DriverVersion = D3D9Data.DriverVersion;
			SurveyData->Hardware->PrimaryVideoCard->VendorID = D3D9Data.VendorId;
			SurveyData->Hardware->PrimaryVideoCard->DeviceID = D3D9Data.DeviceId;
			SurveyData->Hardware->PrimaryVideoCard->SubSysID = D3D9Data.SubSysId;
			SurveyData->Hardware->PrimaryVideoCard->Revision = D3D9Data.Revision;
			SurveyData->Hardware->PrimaryVideoCard->DeviceIdentifier = FromGUID(D3D9Data.DeviceIdentifier);
			SurveyData->Hardware->PrimaryVideoCard->VertexShaderVersionDX9 = (USHORT)LOWORD(D3D9Data.VertexShaderVersion);
			SurveyData->Hardware->PrimaryVideoCard->PixelShaderVersionDX9 = (USHORT)LOWORD(D3D9Data.PixelShaderVersion);
			SurveyData->Hardware->PrimaryMonitor->Width = D3D9Data.DesktopWidth;
			SurveyData->Hardware->PrimaryMonitor->Height = D3D9Data.DesktopHeight;
			SurveyData->Hardware->PrimaryMonitor->RefreshRate = D3D9Data.DesktopRefreshRate;
			SurveyData->Hardware->PrimaryMonitor->BitsPerPixel = D3D9Data.DesktopBitsPerPixel;
			SurveyData->Hardware->TotalMonitors = D3D9Data.TotalMonitors;
			SurveyData->Hardware->PrimaryMonitor->VirtualScreenWidth = System::Windows::Forms::SystemInformation::VirtualScreen.Width;
			SurveyData->Hardware->PrimaryMonitor->VirtualScreenHeight = System::Windows::Forms::SystemInformation::VirtualScreen.Height;
			// IsAdmin check done here to work around namespace pollution problem
			SurveyData->OperatingSystem->IsAdmin = D3D9Data.IsAdmin != 0;

			// Win32_DesktopMonitor actually enumerates an instance per video adapter, not monitor.
			try
			{
				// WMI info
				Searcher->Query = gcnew ObjectQuery( "Select * from Win32_DesktopMonitor" );
				for each ( ManagementObject^ Object in Searcher->Get() )
				{
					SurveyData->Hardware->TotalVideoCards++;
					break;
				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to get Win32_DesktopMonitor WMI entry. " + Ex->ToString() + "\n\n";
			}
			//debugf(TEXT("11 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

			try
			{
				// WMI info
				Searcher->Query = gcnew ObjectQuery( "Select AdapterRAM from Win32_VideoController" );
				for each ( ManagementObject^ Object in Searcher->Get() )
				{
					SurveyData->Hardware->PrimaryVideoCard->TotalVRAM = GetWMIProperty<UINT>(Object, "AdapterRAM", SurveyData->DataCollectionErrorString);
					break;
				}
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to get Win32_VideoController WMI entry. " + Ex->ToString() + "\n\n";
			}
			//debugf(TEXT("12 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

			try
			{
				RecordPhysicalDimensionsOfDesktopMonitor(SurveyData);
			}
			catch(Exception^ Ex)
			{
				SurveyData->DataCollectionErrorString += "Failed to determine physical dimensions of primary monitor. " + Ex->ToString() + "\n\n";
			}
		}
		//debugf(TEXT("13 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		// AppCompat info (will be -1 if not done yet)
		// @TODO - pass this in from the main thread. Not necessarily safe to read config from a thread.
		{
			const TCHAR* AppCompatStr = TEXT("AppCompat");
			const TCHAR* AppCompatCompositeEntryStr = TEXT("CompatLevelComposite");
			const TCHAR* AppCompatCPUEntryStr = TEXT("CompatLevelCPU");
			const TCHAR* AppCompatGPUEntryStr = TEXT("CompatLevelGPU");
			FCompatibilityLevelInfo PreviouslySetCompatLevel(-1,-1,-1);
			GConfig->GetInt( AppCompatStr, AppCompatCompositeEntryStr, (INT&)PreviouslySetCompatLevel.CompositeLevel, GEngineIni );
			GConfig->GetInt( AppCompatStr, AppCompatCPUEntryStr, (INT&)PreviouslySetCompatLevel.CPULevel, GEngineIni );
			GConfig->GetInt( AppCompatStr, AppCompatGPUEntryStr, (INT&)PreviouslySetCompatLevel.GPULevel, GEngineIni );
			SurveyData->Hardware->AppCompatLevelComposite = PreviouslySetCompatLevel.CompositeLevel;
			SurveyData->Hardware->AppCompatLevelCPU = PreviouslySetCompatLevel.CPULevel;
			SurveyData->Hardware->AppCompatLevelGPU = PreviouslySetCompatLevel.GPULevel;
		}
		//debugf(TEXT("14 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		// @TODO - pass this in from the main thread. Not necessarily safe to read config from a thread.
		{
			const TCHAR* HardwareSurveySectionStr = TEXT("HardwareSurvey");
			const TCHAR* HardwareSurveyAttemptedStr = TEXT("SurveysAttempted");
			const TCHAR* HardwareSurveyFailedStr = TEXT("SurveysFailed");
			INT NumSurveysAttempted=0, NumSurveysFailed=0;
			GConfig->GetInt(HardwareSurveySectionStr, HardwareSurveyFailedStr, NumSurveysFailed, GEngineIni);
			GConfig->GetInt(HardwareSurveySectionStr, HardwareSurveyAttemptedStr, NumSurveysAttempted, GEngineIni);
			SurveyData->UDKHeader->SurveysAttempted = NumSurveysAttempted;
			SurveyData->UDKHeader->SurveysFailed = NumSurveysFailed;
		}
		//debugf(TEXT("15 Survey Time: %.3f ms"), (double)(DateTime::UtcNow - SurveyStart).TotalMilliseconds);

		SurveyData->UDKHeader->SurveyTimeMS = (INT)(DateTime::UtcNow - SurveyStart).TotalMilliseconds;
	}
	catch(Exception^ Ex)
	{
		SurveyData->DataCollectionErrorString += "Top-level exception in Hardware survey gathering. " + Ex->ToString() + "\n\n";
	}

	// make sure we return a null string if there was no exception so the backend doesn't get mixed signals.
	if (String::IsNullOrEmpty(SurveyData->DataCollectionErrorString))
	{
		SurveyData->DataCollectionErrorString = nullptr;
	}

	return SurveyData;
}

/** performs the hardware survey and dumps it to disk in the log dir. used for debugging/development */
void PerformHardwareSurveyDumpCLR()
{
	debugf(TEXT("Hardware survey dump requested. Dumping XML to the log directory."));
	// output to an indented stream for readability.
	StreamWriter^ OutStream = File::CreateText(gcnew String(*appGameLogDir())+TEXT("HardwareSurvey.xml"));
	XmlWriterSettings^ XmlSettings = gcnew XmlWriterSettings();
	XmlSettings->Indent = true;
	XmlSettings->OmitXmlDeclaration = true;
	XmlSettings->NewLineOnAttributes = true;
	XmlWriter^ Writer = XmlWriter::Create(OutStream, XmlSettings );

	(gcnew XmlSerializer(UDKSurveyPostProcessor::UDKSurvey::typeid))->Serialize(Writer, RetrieveHardwareInfo());
}

/** Performs the hardware survey and dumps the output into an unmanaged byte array for subsequent HTTP upload. */
void PerformHardwareSurveyCLR(TArray<BYTE>& OutPayload)
{
	// convert to Xml string
	MemoryStream^ MemStream = gcnew MemoryStream();
	(gcnew XmlSerializer(UDKSurveyPostProcessor::UDKSurvey::typeid))->Serialize(XmlWriter::Create(MemStream), RetrieveHardwareInfo());


	// marshal the text into the BYTE array
	array<System::Byte>^ Payload = MemStream->ToArray();
	OutPayload.Empty(Payload->Length+1);
	OutPayload.Add(Payload->Length+1);
	System::Runtime::InteropServices::Marshal::Copy(Payload, 0, (System::IntPtr)OutPayload.GetTypedData(), Payload->Length);
	// NULL terminate the string.
	OutPayload(Payload->Length) = 0;
}
