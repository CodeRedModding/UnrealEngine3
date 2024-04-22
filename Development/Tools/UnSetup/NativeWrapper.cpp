/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include <Windows.h>
#include <d3d9.h>
#include <shlwapi.h>
#include <richedit.h>
#include <gdiplus.h>
#include <stdio.h>
#include <ShlObj.h>
#include <Rpc.h>

#include "resource.h"

using namespace Gdiplus;

// UnSetupNativeWrapper
//	"UDKNativ"
// UnSetup
//	"UDKNET40"
// dotNetFx40_Full_setup
//	"UDKRedis"
// Redist - optional
//	"UDKMagic"
// zip

#define MAX_COMMAND_LINE		1024

static HINSTANCE GlobalInstanceHandle = NULL;
static ULONG_PTR GDIToken = NULL;
static Bitmap* SplashBitmap = NULL;
static bool Ticking = true;

static const wchar_t* UDKNativ = L"UDKNativ";
static const wchar_t* UDKNET40 = L"UDKNET40";
static const wchar_t* UDKRedis = L"UDKRedis";
static const wchar_t* UDKMagic = L"UDKMagic";

// Check for .NET4 being installed
bool CheckDotNet40( void )
{
	DWORD Result = 0;
	DWORD ResultSize = sizeof( Result );
	bool DotNetInstalled = false;

	DWORD Error = SHRegGetValue( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full", L"Install", RRF_RT_DWORD, NULL, &Result, &ResultSize );
	if( Error == ERROR_SUCCESS )
	{
		if( Result == 1 )
		{
			DotNetInstalled = true;
		}
	}

	return( DotNetInstalled );
}

// DialogProc for the splash screen
INT_PTR CALLBACK SplashDialogFunc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_PAINT:
		if( SplashBitmap )
		{
			HDC DeviceContext;
			PAINTSTRUCT PaintInfo;

			DeviceContext = BeginPaint( hwndDlg, &PaintInfo );

			Graphics Gfx( DeviceContext );
			Gfx.DrawImage( SplashBitmap, 0, 0 );

			EndPaint( hwndDlg, &PaintInfo );
		}
		break;

	case WM_MOUSEWHEEL:
	case WM_CLOSE:
		Ticking = false;
		break;
	}

	return( DefWindowProc( hwndDlg, uMsg, wParam, lParam ) );
}

// Display the splash screen
HWND ShowSplashScreen( void )
{
	HWND Splash = NULL;

	GdiplusStartupInput GDIInput;
	GdiplusStartup( &GDIToken, &GDIInput, NULL );

	SplashBitmap = Bitmap::FromResource( GlobalInstanceHandle, MAKEINTRESOURCE( IDR_BMP_SPLASH ) );
	if( SplashBitmap )
	{
		Splash = CreateDialog( NULL, MAKEINTRESOURCE( IDD_DIALOG_SPLASH ), NULL, SplashDialogFunc );
		if( Splash )
		{
			HWND Desktop = GetDesktopWindow();
			RECT DesktopRect = { 0 };
			GetWindowRect( Desktop, &DesktopRect );			
			HMONITOR BestMonitor = MonitorFromRect( &DesktopRect, MONITOR_DEFAULTTONEAREST );

			MONITORINFO MonitorInfo = { 0 };
			MonitorInfo.cbSize = sizeof( MonitorInfo );
			GetMonitorInfo( BestMonitor, &MonitorInfo );

			INT ImageWidth = SplashBitmap->GetWidth();
			INT ImageHeight = SplashBitmap->GetHeight();

			INT ImageX = ( ( MonitorInfo.rcWork.right - MonitorInfo.rcWork.left ) - ImageWidth ) / 2;
			INT ImageY = ( ( MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top ) - ImageHeight ) / 2;

			SetWindowPos( Splash, HWND_TOP, ImageX, ImageY, ImageWidth, ImageHeight, SWP_SHOWWINDOW );
		}
		else
		{
			OutputDebugStringW( L"Failed to create splash dialog\r\n" );
		}
	}
	else
	{
		OutputDebugStringW( L"Failed to create new Bitmap\r\n" );
	}

	return( Splash );
}

// Find a signature in a binary file
DWORD FindSignature( HANDLE Module, const wchar_t* UDKSignature, BOOL& bSignatureFound )
{
	BYTE Buffer[8];
	DWORD BytesRead = 0;

	while( ReadFile( Module, Buffer, 8, &BytesRead, NULL ) && ( BytesRead == 8 ) )
	{
		bSignatureFound = TRUE;

		for( INT Index = 0; Index < 8; Index++ )
		{
			if( Buffer[Index] != ( BYTE )UDKSignature[Index] )
			{
				bSignatureFound = FALSE;
				break;
			}
		}

		if( bSignatureFound )
		{
			return( SetFilePointer( Module, 0, NULL, FILE_CURRENT ) );
		}
	}

	return( ( DWORD )-1 );
}

// Find a signature in a binary file
DWORD FindSignatures( HANDLE Module, const wchar_t* PrimarySignature, BOOL& bPrimarySignatureFound, const wchar_t* SecondarySignature, BOOL& bSecondarySignatureFound )
{
	BYTE Buffer[8];
	DWORD BytesRead = 0;
	bPrimarySignatureFound = FALSE;
	bSecondarySignatureFound = FALSE;

	while( ReadFile( Module, Buffer, 8, &BytesRead, NULL ) && ( BytesRead == 8 ) && !bPrimarySignatureFound && !bSecondarySignatureFound )
	{
		bPrimarySignatureFound = TRUE;
		bSecondarySignatureFound = TRUE;

		for( INT Index = 0; Index < 8; Index++ )
		{
			if( Buffer[Index] != ( BYTE )PrimarySignature[Index] )
			{
				bPrimarySignatureFound = FALSE;
			}

			if( Buffer[Index] != ( BYTE )SecondarySignature[Index] )
			{
				bSecondarySignatureFound = FALSE;
			}

			if( !bPrimarySignatureFound && !bSecondarySignatureFound )
			{
				break;
			}
		}

		if( bPrimarySignatureFound || bSecondarySignatureFound )
		{
			return( SetFilePointer( Module, 0, NULL, FILE_CURRENT ) );
		}
	}

	return( ( DWORD )-1 );
}

// Save a section of a file off to a separate binary file
BOOL SaveFileSection( HANDLE SourceModule, wchar_t* DestinationPath, DWORD StartOffset, DWORD Length )
{
	DWORD BytesRead = 0;
	DWORD BytesWritten = 0;

	// Make sure the file is not read only
	SetFileAttributes( DestinationPath, GetFileAttributes( DestinationPath ) & ~FILE_ATTRIBUTE_READONLY );

	// Write out the file
	HANDLE DestModule = CreateFile( DestinationPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
	if( DestModule != INVALID_HANDLE_VALUE )
	{
		BYTE* Buffer = ( BYTE* )malloc( Length );
		if( Buffer )
		{
			SetFilePointer( SourceModule, StartOffset, NULL, FILE_BEGIN );
			ReadFile( SourceModule, Buffer, Length, &BytesRead, NULL );
		
			if( BytesRead == Length )
			{
				WriteFile( DestModule, Buffer, Length, &BytesWritten, NULL );
			}
			else
			{
				OutputDebugStringW( L"Failed to read all of section\r\n" );
			}

			CloseHandle( DestModule );
			free( Buffer );
		}
		else
		{
			OutputDebugStringW( L"Failed to malloc buffer for section\r\n" );
		}
	}
	else
	{
		OutputDebugStringW( L"Failed to create destination file\r\n" );
	}

	return( BytesWritten == Length );
}

BOOL ExtractSection( wchar_t* ModuleName, wchar_t* DestinationPath, const wchar_t* StartSignature, const wchar_t* EndSignature, const wchar_t* AltEndSignature )
{
	BOOL bSuccess = FALSE;
	BOOL bStartSignatureFound = FALSE;
	BOOL bEndSignatureFound = FALSE;
	BOOL bAltEndSignatureFound = FALSE;
	DWORD SectionStart = 0;
	DWORD SectionEnd = 0;

	// Locate section in the package
	HANDLE Module = CreateFile( ModuleName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if( Module != INVALID_HANDLE_VALUE )
	{
		SectionStart = FindSignature( Module, StartSignature, bStartSignatureFound );
		if( SectionStart != ( DWORD )-1 )
		{
			if( AltEndSignature == NULL )
			{
				SectionEnd = FindSignature( Module, EndSignature, bEndSignatureFound );
			}
			else
			{
				SectionEnd = FindSignatures( Module, EndSignature, bEndSignatureFound, AltEndSignature, bAltEndSignatureFound );
			}

			if( SectionEnd != ( DWORD )-1 )
			{
				if( SectionEnd > SectionStart )
				{
					bSuccess = SaveFileSection( Module, DestinationPath, SectionStart, SectionEnd - 8 - SectionStart );
				}
				else
				{
					OutputDebugStringW( L"Failed to locate valid signatures - end > start\r\n" );
				}
			}
			else
			{
				OutputDebugStringW( L"Failed to locate end signature\r\n" );
			}
		}
		else
		{
			OutputDebugStringW( L"Failed to locate start signature\r\n" );
		}

		CloseHandle( Module );
	}
	else
	{
		OutputDebugStringW( L"Failed to open install package\r\n" );
	}

	return( bSuccess );
}

/**
 * Is this package a UDK installer, or the redist installer?
 */
BOOL IsRedist( wchar_t* ModuleName )
{
	BOOL bIsRedist = FALSE;
	BOOL bIsUDK = FALSE;

	// Locate section in the package
	HANDLE Module = CreateFile( ModuleName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if( Module != INVALID_HANDLE_VALUE )
	{
		FindSignatures( Module, UDKRedis, bIsRedist, UDKMagic, bIsUDK );

		CloseHandle( Module );
	}

	return( bIsRedist );
}

/**
 * Extract the data between the signature markers
 */
BOOL ExtractUnSetup( wchar_t* ModuleName, wchar_t* WorkFolder )
{
	wchar_t BinariesFolder[MAX_PATH] = { 0 }; 
	wchar_t DestinationPath[MAX_PATH] = { 0 }; 
	BOOL bSuccess = FALSE;

	// Always create the required folders
	swprintf_s( BinariesFolder, MAX_PATH, L"%s\\Binaries", WorkFolder );
	CreateDirectory( BinariesFolder, NULL );
	swprintf_s( DestinationPath, MAX_PATH, L"%s\\UnSetup.exe", BinariesFolder );

	bSuccess = ExtractSection( ModuleName, DestinationPath, UDKNativ, UDKNET40, NULL );

	return( bSuccess );
}

/**
 * Extract the data between the signature markers
 */
BOOL ExtractDotNetFx( wchar_t* ModuleName, wchar_t* WorkFolder )
{
	wchar_t DestinationPath[MAX_PATH] = { 0 }; 
	BOOL bSuccess = FALSE;

	// Always create the required folders
	swprintf_s( DestinationPath, MAX_PATH, L"%s\\dotNetFx40_Full_setup.exe", WorkFolder );

	bSuccess = ExtractSection( ModuleName, DestinationPath, UDKNET40, UDKRedis, UDKMagic );

	return( bSuccess );
}

/**
 * Launch an arbitrary application and optionally wait for completion
 */
DWORD Launch( wchar_t* Executable, bool bWait )
{
	DWORD ExitCode = 0;
	wchar_t CommandLine[MAX_COMMAND_LINE] = { 0 };	

	PROCESS_INFORMATION ProcessInfo = { 0 };
	STARTUPINFO StartupInfo = { 0 };

	StartupInfo.cb = sizeof( STARTUPINFO );
	wcscpy_s( CommandLine, MAX_COMMAND_LINE, Executable );

	OutputDebugStringW( L"Launching " );
	OutputDebugStringW( CommandLine );
	OutputDebugStringW( L"\r\n" );
	if( !CreateProcess( NULL, CommandLine, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &StartupInfo, &ProcessInfo ) )
	{
		OutputDebugStringW( L"Failed to start process: " );
		OutputDebugStringW( Executable );
		OutputDebugStringW( L"\r\n" );
		ExitCode = 1;
	}
	else if( bWait )
	{
		bool Waiting = true;

		while( Waiting )
		{
			OutputDebugStringW( L"Waiting...\r\n" );

			// Pump the message loop to keep the progress dialog alive
			MSG Message;
			while( PeekMessage( &Message, NULL, 0, 0, PM_NOREMOVE ) )
			{
				GetMessage( &Message, NULL, 0, 0 );
				TranslateMessage( &Message );
				DispatchMessage( &Message );
			}

			// Check to see if we have finished
			GetExitCodeProcess( ProcessInfo.hProcess, &ExitCode );

			if( ExitCode != STILL_ACTIVE )
			{
				OutputDebugStringW( L"ExitCode != STILL_ALIVE\r\n" );
				Waiting = false;
			}

			Sleep( 500 );
		}

		CloseHandle( ProcessInfo.hProcess );
		CloseHandle( ProcessInfo.hThread );
	}

	return( ExitCode );
}

/**
 * Pump the message loop to make sure messages are processed
 */
void PumpMessageLoop( void )
{
	MSG Message;
	while( PeekMessage( &Message, NULL, 0, 0, PM_NOREMOVE ) )
	{
		GetMessage( &Message, NULL, 0, 0 );
		TranslateMessage( &Message );
		DispatchMessage( &Message );
	}
}

/**
 * Destroy any assets that are allocated
 */
void Cleanup( void )
{
	if( SplashBitmap )
	{
		delete SplashBitmap;
	}

	GdiplusShutdown( GDIToken );
}

/**
 * Check to make sure the OS version is at least Windows XP SP3
 */
BOOL OSVersionValid( void )
{
	OSVERSIONINFOEX VersionInfo = { sizeof( OSVERSIONINFOEX ), 0 };
	if( GetVersionEx( ( OSVERSIONINFO* )&VersionInfo ) )
	{
		// Check for Vista or later
		if( VersionInfo.dwMajorVersion > 5 )
		{
			return( TRUE );
		}

		// Check for NT
		if( VersionInfo.dwMajorVersion < 5 )
		{
			return( FALSE );
		}
			
		// Check for XP
		if( VersionInfo.dwMinorVersion < 1 )
		{
			return( FALSE );
		}

		// Check for service pack 3
		if( VersionInfo.wServicePackMajor < 3 )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * Check to make sure the video card supports SM3 or higher
 */
BOOL CheckShaderModelVersionValid( void )
{
	D3DCAPS9 DeviceCaps = { D3DDEVTYPE_HAL, 0 };

	OutputDebugStringW( L"Direct3DCreate9 ...\r\n" );
	IDirect3D9* Direct3D = Direct3DCreate9( D3D_SDK_VERSION );
	if( Direct3D != NULL )
	{
		OutputDebugStringW( L"GetDeviceCaps ...\r\n" );
		Direct3D->GetDeviceCaps( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &DeviceCaps );
		Direct3D->Release();
	}

	return( ( DeviceCaps.PixelShaderVersion & 0x0000ff00 ) >= 0x0300 );
}

/**
 * Check if UAC is enabled.
 */
BOOL IsUACEnabled( void )
{
	HKEY EnableLUAKey = NULL;
	DWORD EnableLUAValue = 0;
	DWORD EnableLUAValueKeySize = sizeof( EnableLUAValue );
	LONG Result = ERROR_SUCCESS;
	Result = RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System" ), 0, KEY_READ, &EnableLUAKey );
	if( Result == ERROR_SUCCESS )
	{
		Result = RegQueryValueEx( EnableLUAKey, TEXT( "EnableLUA" ), NULL, NULL, ( BYTE* )&EnableLUAValue, &EnableLUAValueKeySize );
		if( Result == ERROR_SUCCESS )
		{
			if( EnableLUAValue != 0 )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * Place the current module name in the restart area of the registry
 */
void SetupRestart( wchar_t* ModuleName )
{
	HKEY RunOnceKey = NULL;
	DWORD Disposition = 0;

	LONG Result = RegCreateKeyEx( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", 0, L"REG_SZ", 0, KEY_ALL_ACCESS, NULL, &RunOnceKey, &Disposition );
	if( Result == ERROR_SUCCESS )
	{
		wchar_t NewModuleName[MAX_COMMAND_LINE] = { 0 };
		wcscat_s( NewModuleName, MAX_COMMAND_LINE, L"\"" );
		wcscat_s( NewModuleName, MAX_COMMAND_LINE, ModuleName );
		wcscat_s( NewModuleName, MAX_COMMAND_LINE, L"\" -runonce" );

		Result = RegSetValueEx( RunOnceKey, L"NameOfApplication", 0, REG_SZ, ( const BYTE* )NewModuleName, ( wcslen( NewModuleName ) + 1 ) * sizeof( wchar_t ) );
		RegCloseKey( RunOnceKey );
	}
}


/**
 * Remove the code that resumes the install after a reboot
 */
void RemoveRestart( void )
{
	HKEY RunOnceKey = NULL;
	DWORD Disposition = 0;

	LONG Result = RegCreateKeyEx( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", 0, L"REG_SZ", 0, KEY_ALL_ACCESS, NULL, &RunOnceKey, &Disposition );
	if( Result == ERROR_SUCCESS )
	{
		RegDeleteValue( RunOnceKey, L"NameOfApplication" );
	}
}

/**
 * Main program flow
 */
INT WINAPI WinMain( HINSTANCE HInstance, HINSTANCE, char* Argument, INT )
{
	wchar_t WorkFolder[MAX_PATH] = { 0 };
	wchar_t ModuleName[MAX_PATH] = { 0 };
	char AppCommandLine[MAX_COMMAND_LINE] = { 0 };
	wchar_t CommandLine[MAX_COMMAND_LINE] = { 0 };

	GlobalInstanceHandle = HInstance;

	// Get the startup executable path
	OutputDebugStringW( L"Getting module name\r\n" );
	GetModuleFileName( NULL, ModuleName, MAX_PATH );
#if _DEBUG
	wcscpy_s( ModuleName, MAX_PATH, L"E:\\depot\\UnrealEngine3\\Binaries\\Redist\\UE3Redist.exe" );
#endif

	OutputDebugStringW( L"Parsing command line\r\n" );

	strcpy_s( AppCommandLine, MAX_COMMAND_LINE, Argument );
	_strlwr_s( AppCommandLine );

	bool bSkipNETCheck = !!strstr( AppCommandLine, "-skipnetcheck" );
	bool bForceNETInstall = !!strstr( AppCommandLine, "-forcenetinstall" );
	bool bSkipDependencies = !!strstr( AppCommandLine, "-skipdependencies" );
	bool bRunAgainAfterReboot = !!strstr( AppCommandLine, "-runonce" );
	bool bProgressOnly = !!strstr( AppCommandLine, "-progressonly" );

	if( !bRunAgainAfterReboot && !bProgressOnly )
	{
		// These tests only need to be done once, and the SM2 check in particular will fail if done during the runonce after a reboot.
		OutputDebugStringW( L"Checking OS version\r\n" );
		if( !OSVersionValid() )
		{
			MessageBox( NULL, L"You must have at least Windows XP Service Pack 3 installed. Installer will now exit.", L"Installation Failed", MB_OK | MB_ICONERROR );
			return( 2 );	
		}

		OutputDebugStringW( L"Checking shader model version\r\n" );
		if( !CheckShaderModelVersionValid() )
		{
			MessageBox( NULL, L"Your video card must support at least Shader Model 3. Installer will now exit.", L"Installation Failed", MB_OK | MB_ICONERROR );
			return( 3 );	
		}
	}

	// Check for UAC disabled, not Administrator case... 
	if( IsUACEnabled() == FALSE )
	{
		if( IsUserAnAdmin() == FALSE )
		{
			// Can't run!
			MessageBox( NULL, L"You must have administrative privileges to run UDKInstall. Contact your system administrator for more information. Installer will now exit.", L"Installation Failed", MB_OK | MB_ICONERROR );
			return( 4 );
		}
	}

	OutputDebugStringW( L"Creating temp folder\r\n" );

	// Get the temp folder to work from
	GetTempPath( MAX_PATH, WorkFolder );
	wcscat_s( WorkFolder, MAX_PATH, L"Epic-" );

	// Uniquify the temp folder
	UUID Guid = { 0 };
	RPC_WSTR GuidString = NULL;
	UuidCreate( &Guid );
	UuidToString( &Guid, &GuidString );
	wcscat_s( WorkFolder, MAX_PATH, ( wchar_t* )GuidString );

	CreateDirectory( WorkFolder, NULL );

	OutputDebugStringW( L"Checking for .NET4\r\n" );

	// Check for .NET4 being installed
	if( !bSkipNETCheck )
	{
		if( !CheckDotNet40() || bForceNETInstall )
		{
			if( ExtractDotNetFx( ModuleName, WorkFolder ) )
			{
				// Setup installation to continue after reboot
				SetupRestart( ModuleName );

				// Launch dotNetFx40_Client_setup.exe from the temp folder
				swprintf_s( CommandLine, MAX_COMMAND_LINE, L"%s\\dotNetFx40_Full_setup.exe", WorkFolder );
				DWORD ExitCode = Launch( CommandLine, true );

				if( ExitCode == 0 || ExitCode == 1614 || ExitCode == 1641 || ExitCode == 3010 )
				{
					// Restart the machine
					if( bProgressOnly )
					{
						Launch( L"Shutdown.exe /r /f /t 10", false );
					}
					else
					{
						Launch( L"Shutdown.exe /r", false );
					}

					return( 1 );
				}
				else
				{
					// Some sort of error was returned, remove the restart and exit
					OutputDebugStringW( L"Error running .NET4 installation\r\n" );
					RemoveRestart();
					return( 1 );
				}
			}
			else
			{
				OutputDebugStringW( L"Failed to extract DotNetFx\r\n" );
				return( 1 );
			}
		}
	}

	OutputDebugStringW( L"Showing splash screen\r\n" );

	// Show splash screen
	HWND Splash = ShowSplashScreen();

	PumpMessageLoop();

	OutputDebugStringW( L"Extracting UnSetup.exe\r\n" );

	// Find and copy out UnSetup.exe to a temp folder
	if( ExtractUnSetup( ModuleName, WorkFolder ) )
	{
		if( swprintf_s( CommandLine, MAX_COMMAND_LINE, L"%s\\Binaries\\UnSetup.exe \"/module=%s\" /SplashHandle=%d /InstallGuid=%s", WorkFolder, ModuleName, ( DWORD )Splash, ( wchar_t* )GuidString ) > 0 )
		{
			if( IsRedist( ModuleName ) )
			{
				wcscat_s( CommandLine, MAX_COMMAND_LINE, L" /Redist" );
			}

			if( bSkipDependencies )
			{
				wcscat_s( CommandLine, MAX_COMMAND_LINE, L" /SkipDependencies" );
			}

			if( bProgressOnly )
			{
				wcscat_s( CommandLine, MAX_COMMAND_LINE, L" /ProgressOnly" );
			}

			Launch( CommandLine, true );
		}
		else
		{
			OutputDebugStringW( L"Command line too long\r\n" );
		}
	}
	else
	{
		SendMessage( Splash, WM_CLOSE, 0, 0 );
		MessageBox( NULL, L"Failed to extract UnSetup.exe", L"Installation Failed", MB_OK | MB_ICONERROR );
	}

	OutputDebugStringW( L"Exiting\r\n" );

	Cleanup();
	return( 0 );
}

// end
