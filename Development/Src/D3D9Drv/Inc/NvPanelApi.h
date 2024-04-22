/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/

#ifndef __NVPANELAPI_H__
#define __NVPANELAPI_H__

/*
//
//  Copyright (c) 2003-2004 NVIDIA Corporation. All rights reserved.
//
//  This software may not, in whole or in part, be copied through any means,
//  mechanical, electromechanical, or otherwise, without the express permission
//  of NVIDIA Corporation.
//
//  Information furnished is believed to be accurate and reliable. 
//  However, NVIDIA assumes no responsibility for the consequences of use of
//  such information nor for any infringement of patents or other rights of 
//  third parties, which may result from its use.
//
//  No License is granted by implication or otherwise under any patent or 
//  patent rights of NVIDIA Corporation.
//
//  
//  This header file contains declaratios for externally accessible 
//  NVIDIA CONTROL PANEL API methods. For detailed description of the API
//  see "NVIDIA Corporation NVCPL.DLL API Manual".
//
*/

#ifdef __cplusplus
    extern "C" {
#endif

#include "NvApiError.h"

#ifndef IN
#   define IN
#endif//IN

#ifndef OUT
#   define OUT
#endif//OUT

#ifndef NVAPIENTRY
#   define NVAPIENTRY APIENTRY
#endif//NVAPIENTRY        

#ifndef PGAMMARAMP
// PGAMMARAMP is defined in Windows DDK winddi.h header:
//    typedef struct _GAMMARAMP 
//    {
//        WORD  Red[256];
//        WORD  Green[256];
//        WORD  Blue[256];
//    } GAMMARAMP, *PGAMMARAMP;
#   define GAMMARAMP  void
#   define PGAMMARAMP void*
#   define REDEFINED_PGAMMARAMP
#endif//PGAMMARAMP

///////////////////////////////////////////////////////////////////////

#define MAX_NVMONIKER      16   // Maximum number of characters in device monker strings
#define MAX_NVDISPLAYNAME  256  // Maximum length for user-friendly display names

//---------------------------------------------------------------------
// User Interface Functions
//---------------------------------------------------------------------

void NVAPIENTRY ShellExecuteLoadNVPanel( IN LPCSTR pszWindowsDeviceName );
typedef void (NVAPIENTRY* fShellExecuteLoadNVPanel)( IN LPCSTR pszWindowsDeviceName );


//---------------------------------------------------------------------
// Display Selection Functions
//---------------------------------------------------------------------

BOOL __cdecl NvSelectDisplayDevice( IN UINT nWindowsMonitorNumber );
typedef BOOL (__cdecl* fNvSelectDisplayDevice)( IN UINT nWindowsMonitorNumber );


//---------------------------------------------------------------------
// Display Enumeration Functions
//---------------------------------------------------------------------

// For entire GPU:
BOOL NVAPIENTRY NvCplGetRealConnectedDevicesString( OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer, IN BOOL bOnlyActive );
typedef BOOL (NVAPIENTRY* fNvCplGetRealConnectedDevicesString)( OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer, IN BOOL bOnlyActive );

BOOL NVAPIENTRY NvCplGetActiveDevicesString( OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer );
typedef BOOL (NVAPIENTRY* fNvCplGetActiveDevicesString)( OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer );

// For individual DualView head:
BOOL NVAPIENTRY NvCplGetMSOrdinalDeviceString( IN DWORD nWindowsMonitorNumber, OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer );
typedef BOOL (NVAPIENTRY* fNvCplGetMSOrdinalDeviceString)( IN DWORD nWindowsMonitorNumber, OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer );


//---------------------------------------------------------------------
// Display Detection Functions
//---------------------------------------------------------------------

// Flags for NvCplRefreshConnectedDevices
#define NVREFRESH_NONINTRUSIVE 0x00000001 // Performs less exhaustive search and does not
                                          // detach any active display heads which have 
                                          // been physically disconnected since detection.
#define NVREFRESH_SYSTEMWIDE   0x00000002 // Performs refresh for all graphics adapter.  If not
                                          // specified, the selected graphics adapter is used.

BOOL NVAPIENTRY NvCplRefreshConnectedDevices( IN DWORD dwFlags ); // dwFlags is NVREFRESH_* bitmask
typedef BOOL (NVAPIENTRY* fNvCplRefreshConnectedDevices)( IN DWORD dwFlags );


//---------------------------------------------------------------------
// Display Information Functions
//---------------------------------------------------------------------

// Single or Multi-Monitor Display Mode
typedef enum NVDISPLAYMODE
{
    NVDISPLAYMODE_NONE        = -1,         // No Display (or Display Mode Unknown)
    NVDISPLAYMODE_STANDARD    =  0,         // Single-Display Mode
    NVDISPLAYMODE_CLONE       =  1,         // Clone Mode
    NVDISPLAYMODE_HSPAN       =  2,         // Horizontal Span
    NVDISPLAYMODE_VSPAN       =  3,         // Vertical Span
    NVDISPLAYMODE_DUALVIEW    =  4,         // DualView
} NVDISPLAYMODE;

// Display Type
typedef enum NVDISPLAYTYPE // upper nibble indicates device class (for example: 0x2 for DFPs)
{
    NVDISPLAYTYPE_NONE        =     -1,     // No Display (or Display Type Unknown)
    NVDISPLAYTYPE_CRT         = 0x1000,     // Cathode Ray Tube (CRT class from 0x1...)
    NVDISPLAYTYPE_DFP         = 0x2000,     // Digital Flat Panel (DFP class from 0x2...)
    NVDISPLAYTYPE_DFP_LAPTOP  = 0x2001,     //   Subtype: Laptop Display Panel
    NVDISPLAYTYPE_TV          = 0x3000,     // Television Set (TV class from 0x3...)
    NVDISPLAYTYPE_TV_HDTV     = 0x3001,     //   Subtype: High-Definition Television
} NVDISPLAYTYPE;
#define NVDISPLAYTYPE_CLASS_MASK 0xF000     // Mask for device class checks

// TV Format
typedef enum NVTVFORMAT
{
    NVTVFORMAT_NONE           =  -1,        // No Format (field does not apply to current device)
    NVTVFORMAT_NTSC_M         =   0,        // NTSC/M
    NVTVFORMAT_NTSC_J         =   1,        // NTSC/J
    NVTVFORMAT_PAL_M          =   2,        // PAL/M
    NVTVFORMAT_PAL_A          =   3,        // PAL/B, D, G, H, I
    NVTVFORMAT_PAL_N          =   4,        // PAL/N
    NVTVFORMAT_PAL_NC         =   5,        // PAL/NC
    NVTVFORMAT_HD576i         =   8,        // HDTV 576i
    NVTVFORMAT_HD480i         =   9,        // HDTV 480i
    NVTVFORMAT_HD480p         =  10,        // HDTV 480p
    NVTVFORMAT_HD576p         =  11,        // HDTV 576p
    NVTVFORMAT_HD720p         =  12,        // HDTV 720p
    NVTVFORMAT_HD1080i        =  13,        // HDTV 1080i
    NVTVFORMAT_HD1080p        =  14,        // HDTV 1080p
    NVTVFORMAT_HD720i         =  16,        // HDTV 720i
} NVTVFORMAT;

// TV Format Aliases
#define NVTVFORMAT_D1   NVTVFORMAT_HD480i   // D1 SCART
#define NVTVFORMAT_D2   NVTVFORMAT_HD480p   // D2 SCART
#define NVTVFORMAT_D3   NVTVFORMAT_HD1080i  // D3 SCART
#define NVTVFORMAT_D4   NVTVFORMAT_HD720p   // D4 SCART
#define NVTVFORMAT_D5   NVTVFORMAT_HD1080p  // D5 SCART

// TV Connector Types
#define NVTVCONNECTOR_UNKNOWN           0x80000000  // Unknown connector(s)
#define NVTVCONNECTOR_COMPOSITE         0x00000001  // Composite
#define NVTVCONNECTOR_SVIDEO            0x00000002  // S-Video 
#define NVTVCONNECTOR_COMPONENT         0x00000004  // Component
#define NVTVCONNECTOR_EIAJ4120          0x00000008  // EIAJ-4120 
#define NVTVCONNECTOR_EIAJ4120CVBSBLUE  0x00000010  // EIAJ-4120-CVBS
#define NVTVCONNECTOR_SCART             0x00000020  // SCART

// Flat Panel Scaling Mode
typedef enum NVDFPSCALING
{
    NVDFPSCALING_NONE         =  -1,        // No Scaling (or Scaling Unknown)
    NVDFPSCALING_NATIVE       =   1,        // Monitor Scaling
    NVDFPSCALING_SCALED       =   2,        // Scaling
    NVDFPSCALING_CENTERED     =   3,        // Centering
    NVDFPSCALING_SCALEDASPECT =   5,        // Scaling (Fixed Aspect Ratio)
} NVDFPSCALING;

// Graphics Board Product Family
typedef enum NVBOARDTYPE
{
    NVBOARDTYPE_GEFORCE        =   0,        // Geforce board
    NVBOARDTYPE_QUADRO         =   1,        // Quadro board
    NVBOARDTYPE_NVS            =   2,        // NVS board
} NVBOARDTYPE;

// NVDISPLAYINFO.dwInputFields1 and .dwOutputFields1 bitfields
#define NVDISPLAYINFO1_ALL                  0xffffffff  // special: all fields valid
#define NVDISPLAYINFO1_WINDOWSDEVICENAME    0x00000001  // szWindowsDeviceName valid
#define NVDISPLAYINFO1_ADAPTERNAME          0x00000002  // szAdapterName valid
#define NVDISPLAYINFO1_DRIVERVERSION        0x00000004  // szDriverVersion valid
#define NVDISPLAYINFO1_DISPLAYMODE          0x00000008  // nDisplayMode valid
#define NVDISPLAYINFO1_WINDOWSMONITORNUMBER 0x00000010  // dwWindowsMonitorNumber valid
#define NVDISPLAYINFO1_DISPLAYHEADINDEX     0x00000020  // nDisplayHeadIndex valid
#define NVDISPLAYINFO1_DISPLAYISPRIMARY     0x00000040  // bDisplayIsPrimary valid
#define NVDISPLAYINFO1_DISPLAYNAME          0x00000080  // szDisplayName valid
#define NVDISPLAYINFO1_VENDORNAME           0x00000100  // szVendorName valid
#define NVDISPLAYINFO1_MODELNAME            0x00000200  // szModelName valid
#define NVDISPLAYINFO1_GENERICNAME          0x00000400  // szGenericName valid
#define NVDISPLAYINFO1_UNIQUEID             0x00000800  // dwUniqueId valid
#define NVDISPLAYINFO1_DISPLAYTYPE          0x00001000  // nDisplayType valid
#define NVDISPLAYINFO1_DISPLAYWIDTH         0x00002000  // mmDisplayWidth valid
#define NVDISPLAYINFO1_DISPLAYHEIGHT        0x00004000  // mmDisplayHeight valid
#define NVDISPLAYINFO1_GAMMACHARACTERISTIC  0x00008000  // fGammaCharacteristic valid
#define NVDISPLAYINFO1_OPTIMALMODE          0x00010000  // dwOptimal... fields valid
#define NVDISPLAYINFO1_MAXIMUMSAFEMODE      0x00020000  // dwMaximumSafe... fields valid
#define NVDISPLAYINFO1_BITSPERPEL           0x00040000  // dwBitsPerPel valid
#define NVDISPLAYINFO1_PELSWIDTH            0x00080000  // dwPelsWidth valid
#define NVDISPLAYINFO1_PELSHEIGHT           0x00100000  // dwPelsHeight valid
#define NVDISPLAYINFO1_DISPLAYFREQUENCY     0x00200000  // dwDisplayFrequency valid
#define NVDISPLAYINFO1_DISPLAYRECT          0x00400000  // rcDisplayRect valid (rcDisplayRect.TopLeft on write)
#define NVDISPLAYINFO1_VISIBLEPELSWIDTH     0x00800000  // dwVisiblePelsWidth valid
#define NVDISPLAYINFO1_VISIBLEPELSHEIGHT    0x01000000  // dwVisiblePelsHeight valid
#define NVDISPLAYINFO1_DEGREESROTATION      0x02000000  // dwDegreesRotation valid
#define NVDISPLAYINFO1_TVFORMAT             0x04000000  // nTvFormat valid
#define NVDISPLAYINFO1_DFPSCALING           0x08000000  // nDfpScaling valid
#define NVDISPLAYINFO1_TVCONNECTORTYPES     0x10000000  // dwTVConnectorTypes valid
#define NVDISPLAYINFO1_CURRENTCONNECTORTYPE 0x20000000  // dwCurrentConnectorType is valid
#define NVDISPLAYINFO1_BOARDTYPE            0x40000000  // dwBoardType is valid
#define NVDISPLAYINFO1_DISPLAYINSTANCECOUNT 0x80000000  // dwDisplayInstance and dwDisplayInstanceCount are valid

// NVDISPLAYINFO.dwInputFields2 and .dwOutputFields2 bitfields
#define NVDISPLAYINFO2_ALL                  0xffffffff  // special: all fields valid
#define NVDISPLAYINFO2_PRODUCTNAME          0x00000001  // szProductName valid
#define NVDISPLAYINFO2_DVIOVERHDTV          0x00000002  // bDVIOverHDTV valid
#define NVDISPLAYINFO2_CONNECTEDMONIKER     0x00000004  // szConnectedMoniker valid
#define NVDISPLAYINFO2_ACTIVEMONIKER        0x00000008  // szActiveMoniker valid
#define NVDISPLAYINFO2_SLIENABLED           0x00000010  // bSLIEnabled valid - SLI is turned on/active
#define NVDISPLAYINFO2_SLICONNECTOR         0x00000020  // bSLIConnector valid - SLI connector exists
#define NVDISPLAYINFO2_SLICAPABLE           0x00000040  // bSLICapable valid - SLI can be enabled for this display
#define NVDISPLAYINFO2_CUSTOMNAME           0x00000080  // szCustomName valid

// Display Information Parameter Block
typedef struct tagNVDISPLAYINFO
{
    DWORD cbSize;                           // Size of the NVDISPLAYINFO structure (in bytes), 
                                            //  set this field to sizeof(NVDISPLAYINFO) to indicate version level of structure
    DWORD dwInputFields1;                   // Specifies which members of structure should be used on input to function (see NVDISPLAYINFO1_*)
    DWORD dwOutputFields1;                  // Specifies which members of structure were processed as result of the call
    DWORD dwInputFields2;                   // Specifies which members of structure should be used on input to function (see NVDISPLAYINFO2_*)
    DWORD dwOutputFields2;                  // Specifies which members of structure were processed as result of the call
    
    char  szWindowsDeviceName[MAX_PATH];    // Device name for use with CreateDC (for example: ".\\DISPLAY1")
    char  szAdapterName[MAX_NVDISPLAYNAME]; // User friendly name for the associated NVIDIA graphics card (for example: GeForce FX 5200 Ultra)
    char  szDriverVersion[64];              // Display driver version string for device (for example: "6.14.10.6003")
    NVDISPLAYMODE nDisplayMode;             // Display mode for head on adapter (for example: Clone, HSpan, DualView)
    DWORD dwWindowsMonitorNumber;           // Windows monitor number for adapter (numbers listed in Microsoft Display Panel)
    int   nDisplayHeadIndex;                // Head index for display on adapter
    BOOL  bDisplayIsPrimary;                // TRUE if display head is primary on adapter

    char  szDisplayName[MAX_NVDISPLAYNAME]; // User friendly name for the display device, may reflect user overrides (for example: "EIZO L685")
    char  szVendorName[MAX_NVDISPLAYNAME];  // Vendor name for display device if available (for example: "EIZO")
    char  szModelName[MAX_NVDISPLAYNAME];   // Model name for display device if available (for example: "EIZ1728")
    char  szGenericName[MAX_NVDISPLAYNAME]; // Generic name for display device type (for example: "Digital Display")
    DWORD dwUniqueId;                       // Unique identifier for display device, including serial number, zero if not available

    NVDISPLAYTYPE nDisplayType;             // Type of the display device (for example: CRT, DFP, or TV)
    DWORD mmDisplayWidth;                   // Width of maximum visible display surface or zero if unknown (in millimeters)
    DWORD mmDisplayHeight;                  // Height of maximum visible display surface or zero if unknown (in millimeters)
    float fGammaCharacteristic;             // Gamma transfer characteristic for monitor (for example: 2.2)
    
    DWORD dwOptimalPelsWidth;               // Width of display surface in optimal display mode (not necessarily highest resolution)
    DWORD dwOptimalPelsHeight;              // Height of display surface in optimal display mode (not necessarily highest resolution)
    DWORD dwOptimalDisplayFrequency;        // Refresh frequency in optimal display mode (not necessarily highest resolution)

    DWORD dwMaximumSafePelsWidth;           // Width of display surface in maximum safe display mode (not necessarily highest resolution)
    DWORD dwMaximumSafePelsHeight;          // Height of display surface in maximum safe display mode (not necessarily highest resolution)
    DWORD dwMaximumSafeDisplayFrequency;    // Refresh frequency in maximum safe display mode (not necessarily highest resolution)

    DWORD dwBitsPerPel;                     // Color resolution of the display device (for example: 8 bits for 256 colors)
    DWORD dwPelsWidth;                      // Width of the available display surface, including any pannable area (in pixels)
    DWORD dwPelsHeight;                     // Height of the available display surface, including any pannable area (in pixels)
    DWORD dwDisplayFrequency;               // Refresh frequency of the display device (in hertz)

    RECT  rcDisplayRect;                    // Desktop rectangle for display surface (considers DualView and head offset)
    DWORD dwVisiblePelsWidth;               // Width of the visible display surface, excluding any pannable area (in pixels)
    DWORD dwVisiblePelsHeight;              // Height of the visible display surface, excluding any pannable area (in pixels)

    DWORD dwDegreesRotation;                // Rotation angle of display surface (in degrees)
    NVTVFORMAT nTvFormat;                   // Television video signal format (for example: NTSC/M or HDTV 1080i)
    NVDFPSCALING nDfpScaling;               // Digital Flat Panel scaling mode (for example: Monitor Native)

    DWORD dwTVConnectorTypes;               // Television connectors (NVTVCONNECTOR_* bitmask)
    DWORD dwCurrentConnectorType;           // Television active connector (NVTVCONNECTOR_* bitmask)
    DWORD dwBoardType;                      // Type of graphics board (NVBOARDTYPE_* enumeration)

    DWORD dwDisplayInstance;                // Display instance number (instance of szDisplayName) or zero if indeterminant
    DWORD dwDisplayInstanceCount;           // Display instance count (instances of szDisplayName) or zero if indeterminant

    char  szProductName[MAX_NVDISPLAYNAME]; // Product name for display device if available, bypasses user customization of szDisplayName (for example: "EIZO L685")
    BOOL  bDVIOverHDTV;                     // DVI over HDTV video for digital display
    char  szConnectedMoniker[MAX_NVMONIKER];// Device moniker for display based on physically connected devices (empty if not connected)
    char  szActiveMoniker[MAX_NVMONIKER];   // Device moniker for display based on active display outputs (e.g. those attached to desktop, empty if not attached)

    BOOL  bSLIEnabled;                      // SLI is turned on and active
    BOOL  bSLIConnector;                    // SLI connector exists
    BOOL  bSLICapable;                      // SLI can be enabled for this display

    char  szCustomName[MAX_NVDISPLAYNAME];  // Custom name for display device type if available (for example: "Dell Monitor on Left")

} NVDISPLAYINFO;

BOOL NVAPIENTRY NvGetDisplayInfo( IN LPCSTR pszUserDisplay, OUT NVDISPLAYINFO* pDisplayInfo );
typedef BOOL (NVAPIENTRY* fNvGetDisplayInfo)( IN LPCSTR pszUserDisplay, OUT NVDISPLAYINFO* pDisplayInfo );

//---------------------------------------------------------------------
// Display Name Functions
//---------------------------------------------------------------------

NVRESULT NVAPIENTRY NvGetDisplayCustomName( IN LPCSTR pszUserDisplay, OUT LPSTR pszTextBuffer, IN DWORD cbTextBuffer );
typedef NVRESULT (NVAPIENTRY* fNvGetDisplayCustomName)( IN LPCSTR pszUserDisplay, OUT LPSTR pszTextBuffer, IN DWORD cbTextBuffer );

NVRESULT NVAPIENTRY NvSetDisplayCustomName( IN LPCSTR pszUserDisplay, IN LPCSTR pszTextBuffer );
typedef NVRESULT (NVAPIENTRY* fNvSetDisplayCustomName)( IN LPCSTR pszUserDisplay, IN LPCSTR pszTextBuffer );


//---------------------------------------------------------------------
// Display Mode Functions
//---------------------------------------------------------------------

#define EDS_ALLMODES 0xffff // Flag to enumeration functions that disables mode filtering against adapter or display capabilities

NVRESULT NVAPIENTRY NvEnumDisplaySettings( IN LPCSTR pszUserDisplay, IN NVDISPLAYMODE displayMode, IN DWORD dwDevModeSize, OUT DEVMODE* pDevModes, IN OUT DWORD* pdwNumDevModes, IN DWORD dwFlags );
typedef NVRESULT (NVAPIENTRY* fNvEnumDisplaySettings)( IN LPCSTR pszUserDisplay, IN NVDISPLAYMODE displayMode, IN DWORD dwDevModeSize, OUT DEVMODE* pDevModes, IN OUT DWORD* pdwNumDevModes, IN DWORD dwFlags );

NVRESULT NVAPIENTRY NvGetLastDisplaySettings( IN LPCSTR pszUserDisplay, IN NVDISPLAYMODE displayMode, OUT DEVMODE* pDevMode, IN DWORD dwFlags );
typedef NVRESULT (NVAPIENTRY* fNvGetLastDisplaySettings)( IN LPCSTR pszUserDisplay, IN NVDISPLAYMODE displayMode, OUT DEVMODE* pDevMode, IN DWORD dwFlags );

NVRESULT NVAPIENTRY NvGetDefaultDisplaySettings( IN LPCSTR pszUserDisplay, IN NVDISPLAYMODE displayMode, OUT DEVMODE* pDevMode, IN DWORD dwFlags );
typedef NVRESULT (NVAPIENTRY* fNvGetDefaultDisplaySettings)( IN LPCSTR pszUserDisplay, IN NVDISPLAYMODE displayMode, OUT DEVMODE* pDevMode, IN DWORD dwFlags );


//---------------------------------------------------------------------
// Physical Connector Information Functions (requires special NV4X+ BIOS)
//---------------------------------------------------------------------

// Physical Connector Layout
typedef enum NVCONNECTORLAYOUT
{
    NVCONNECTORLAYOUT_UNKNOWN                          =  0, // Unknown connector layout
    NVCONNECTORLAYOUT_CARD_SINGLESLOT                  =  1, // Single slot add-in card
    NVCONNECTORLAYOUT_CARD_DOUBLESLOT                  =  2, // Double slot add-in card, used for tall fan sinks and daughtercards
    NVCONNECTORLAYOUT_CARD_MOBILE_MXM                  =  3, // Mobile add-in card conforming to the MXM module standard
    NVCONNECTORLAYOUT_CARD_MOBILE_OEM                  =  4, // Mobile add-in card for OEMs, typically used to add LVDS-SPWG connectors
    NVCONNECTORLAYOUT_MOBILE_BACK                      =  5, // Mobile system with all displays on back of system
    NVCONNECTORLAYOUT_MOBILE_BACK_LEFT                 =  6, // Mobile system with all displays on back and left of system
    NVCONNECTORLAYOUT_MOBILE_BACK_DOCK                 =  7, // Mobile system with all displays on back of system or on the docking station
    NVCONNECTORLAYOUT_NFORCE_STANDARD                  =  8, // System with standard nForce connector layout
} NVCONNECTORLAYOUT;

// Physical Connector Type
typedef enum NVCONNECTORTYPE
{
    NVCONNECTORTYPE_UNKNOWN                            =  0, // Unknown connector type
    NVCONNECTORTYPE_UNCLASSIFIED_ANALOG                =  1, // Unclassifiable Analog connector
    NVCONNECTORTYPE_UNCLASSIFIED_DIGITAL               =  2, // Unclassifiable Digital connector
    NVCONNECTORTYPE_UNCLASSIFIED_TV                    =  3, // Unclassifiable TV connector
    NVCONNECTORTYPE_UNCLASSIFIED_LVDS                  =  4, // Unclassifiable LVDS connector (as in laptop panels)
    NVCONNECTORTYPE_VGA                                =  5, // VGA 15-pin connector
    NVCONNECTORTYPE_DVI_A                              =  6, // DVI-A - DVI Analog
    NVCONNECTORTYPE_DVI_D                              =  7, // DVI-D - DVI Digital
    NVCONNECTORTYPE_DVI_I                              =  8, // DVI-I - DVI Integrated
    NVCONNECTORTYPE_DVI_I_TV_SVIDEO                    =  9, // DVI-I-TV S-Video
    NVCONNECTORTYPE_DVI_I_TV_COMPOSITE                 = 10, // DVI-I-TV Composite
    NVCONNECTORTYPE_DVI_I_TV_SVIDEO_BREAKOUT_COMPOSITE = 11, // DVI-I-TV S-Video Breakout (Composite)
    NVCONNECTORTYPE_LFH_DVI_I_1                        = 12, // LFH-DVI-I-1 - 60-pin LFH connector (as in Quadro NVS series)
    NVCONNECTORTYPE_LFH_DVI_I_2                        = 13, // LFH-DVI-I-2 - 60-pin LFH connector
    NVCONNECTORTYPE_LVDS_SPWG                          = 14, // LVDS-SPWG - Low Voltage Differential Signaling (as in laptop panels)
    NVCONNECTORTYPE_LVDS_OEM                           = 15, // LVDS-OEM  - Low Voltage Differential Signaling
    NVCONNECTORTYPE_TMDS_OEM                           = 16, // TMDS-OEM  - Transition Minimized Differential Signaling
    NVCONNECTORTYPE_ADC                                = 17, // Apple Display Connector (ADC)
    NVCONNECTORTYPE_TV_COMPOSITE                       = 18, // TV - Composite Out
    NVCONNECTORTYPE_TV_SVIDEO                          = 19, // TV - S-Video Out
    NVCONNECTORTYPE_TV_SVIDEO_BREAKOUT_COMPOSITE       = 20, // TV - S-Video Breakout (Composite)
    NVCONNECTORTYPE_TV_SCART                           = 21, // TV - SCART D-Connector
    NVCONNECTORTYPE_PC_YPRPB                           = 22, // Personal Cinema - YPrPb
    NVCONNECTORTYPE_PC_SVIDEO                          = 23, // Personal Cinema - S-Video
    NVCONNECTORTYPE_PC_COMPOSITE                       = 24, // Personal Cinema - Composite
    NVCONNECTORTYPE_STEREO                             = 25, // 3-Pin DIN Stereo Connector
} NVCONNECTORTYPE;

// Physical Connector Flags
#define NVCONNECTORFLAG_REMOVEABLE  0x00000001  // Connector supports removeable devices (an example of a fixed connector is an internal laptop display)
#define NVCONNECTORFLAG_DIGITAL     0x00000002  // Connector supports digital displays (ex. DFPs)
#define NVCONNECTORFLAG_ANALOG      0x00000004  // Connector supports analog displays (ex. CRTs)
#define NVCONNECTORFLAG_TV          0x00000008  // Connector supports TV sets
#define NVCONNECTORFLAG_HDTV        0x00000010  // Connector supports HDTV sets

// Physical Connector Information
typedef struct NVCONNECTORINFO
{
    DWORD              cbSize;                  // Size of the NVCONNECTORINFO structure in bytes (on input)
    NVCONNECTORLAYOUT  nConnectorLayout;        // Connector layout
    DWORD              dwConnectorCount;        // Number of connectors on card
} NVCONNECTORINFO;

// Physical Connector Details
typedef struct tagNVCONNECTORDETAIL
{
    DWORD              cbSize;                  // Size of the NVCONNECTORDETAIL structure in bytes (on input)
    NVCONNECTORTYPE    nConnectorType;          // Connector type
    DWORD              dwConnectorLocation;     // Connector location (for add-in cards zero means the
                                                // connector furthest from the motherboard)
    DWORD              dwFlags;                 // Connector flags (see NVCONNECTORFLAG_* definitions)
} NVCONNECTORDETAIL;

NVRESULT NVAPIENTRY NvGetPhysicalConnectorInfo( IN UINT nWindowsMonitorNumber, OUT NVCONNECTORINFO* pConnectorInfo );
typedef NVRESULT (NVAPIENTRY* fNvGetPhysicalConnectorInfo)( IN UINT nWindowsMonitorNumber, OUT NVCONNECTORINFO* pConnectorInfo );

NVRESULT NVAPIENTRY NvEnumPhysicalConnectorDetails( IN UINT nWindowsMonitorNumber, IN DWORD dwConnectorIndex, OUT NVCONNECTORDETAIL* pConnectorDetail );
typedef NVRESULT (NVAPIENTRY* fNvEnumPhysicalConnectorDetails)( IN UINT nWindowsMonitorNumber, IN DWORD dwConnectorIndex, OUT NVCONNECTORDETAIL* pConnectorDetail );


//---------------------------------------------------------------------
// Desktop Configuration Functions
//---------------------------------------------------------------------

DWORD NVAPIENTRY dtcfgex( IN OUT LPSTR lpszCmdLine );
typedef DWORD (NVAPIENTRY* fdtcfgex)( IN OUT LPSTR lpszCmdLine );

DWORD NVAPIENTRY GetdtcfgLastError();
typedef DWORD (NVAPIENTRY* fGetdtcfgLastError)();

DWORD NVAPIENTRY GetdtcfgLastErrorEx( IN OUT LPSTR lpszCmdLine, IN OUT DWORD *pdwCmdLineSize );
typedef DWORD (NVAPIENTRY* fGetdtcfgLastErrorEx)( IN OUT LPSTR lpszCmdLine, IN OUT DWORD *pdwCmdLineSize );


//---------------------------------------------------------------------
// Gamma Ramp Functions
//---------------------------------------------------------------------

typedef enum NVCOLORAPPLY
{
    NVCOLORAPPLY_DESKTOP,          // Apply color settings to Desktop
    NVCOLORAPPLY_OVERLAYVMR,       // Apply color settings to Overlay/Video Mirroring
    NVCOLORAPPLY_FULLSCREENVIDEO,  // Apply color settings to Fullscreen Video
    NVCOLORAPPLY_COUNT             // Number of apply color settings targets
} NVCOLORAPPLY;

BOOL NVAPIENTRY NvColorGetGammaRampEx( IN LPCSTR pszUserDisplay, OUT GAMMARAMP* pGammaRamp, IN NVCOLORAPPLY applyFrom );
typedef BOOL (NVAPIENTRY* fNvColorGetGammaRampEx)( IN LPCSTR pszUserDisplay, OUT GAMMARAMP* pGammaRamp, IN NVCOLORAPPLY applyFrom );

BOOL NVAPIENTRY NvColorSetGammaRampEx( IN LPCSTR pszUserDisplay, IN const GAMMARAMP* pGammaRamp, IN NVCOLORAPPLY applyTo );
typedef BOOL (NVAPIENTRY* fNvColorSetGammaRampEx)( IN LPCSTR pszUserDisplay, IN const GAMMARAMP* pGammaRamp, IN NVCOLORAPPLY applyTo );


//---------------------------------------------------------------------
// Video Functions
//---------------------------------------------------------------------

BOOL __cdecl NvGetFullScreenVideoMirroringEnabled( IN LPCSTR pszUserDisplay, OUT BOOL* pbEnabled );
typedef BOOL (__cdecl* fNvGetFullScreenVideoMirroringEnabled)( IN LPCSTR pszUserDisplay, OUT BOOL* pbEnabled );

BOOL __cdecl NvSetFullScreenVideoMirroringEnabled( IN LPCSTR pszUserDisplay, IN BOOL bEnabled );
typedef BOOL (__cdecl* fNvSetFullScreenVideoMirroringEnabled)( IN LPCSTR pszUserDisplay, IN BOOL bEnabled );


//---------------------------------------------------------------------
// Power Management Functions
//---------------------------------------------------------------------

BOOL __cdecl NvCplIsExternalPowerConnectorAttached( IN BOOL* pbAttached );
typedef BOOL (__cdecl* fNvCplIsExternalPowerConnectorAttached)( IN BOOL* pbAttached );

BOOL NVAPIENTRY nvGetPwrMzrLevel( OUT DWORD* pdwBatteryLevel, OUT DWORD* pdwACLevel );
typedef BOOL (NVAPIENTRY* fnvGetPwrMzrLevel)( OUT DWORD* pdwBatteryLevel, OUT DWORD* pdwACLevel );

BOOL NVAPIENTRY nvSetPwrMzrLevel( IN DWORD* pdwBatteryLevel, IN DWORD* pdwACLevel );
typedef BOOL (NVAPIENTRY* fnvSetPwrMzrLevel)( IN DWORD* pdwBatteryLevel, IN DWORD* pdwACLevel );


//---------------------------------------------------------------------
// Temperature Monitoring Functions
//---------------------------------------------------------------------

BOOL __cdecl NvCplGetThermalSettings( IN UINT nWindowsMonitorNumber, OUT DWORD* pdwCoreTemp, OUT DWORD* pdwAmbientTemp, OUT DWORD* pdwUpperLimit );
typedef BOOL (__cdecl* fNvCplGetThermalSettings)( IN UINT nWindowsMonitorNumber, OUT DWORD* pdwCoreTemp, OUT DWORD* pdwAmbientTemp, OUT DWORD* pdwUpperLimit );


//---------------------------------------------------------------------
// Data Control Functions
//---------------------------------------------------------------------

#define NVCPL_API_AGP_BUS_MODE                   1    // Graphics card connection to system
                                                      //  Values are:
                                                      //   1 = PCI
                                                      //   4 = AGP
                                                      //   8 = PCI Express
#define NVCPL_API_VIDEO_RAM_SIZE                 2    // Graphics card video RAM in megabytes
#define NVCPL_API_TX_RATE                        3    // Graphics card AGP bus rate (1X, 2X, ...)

#define NVCPL_API_CURRENT_AA_VALUE               4    // Graphics card anti-aliasing setting.
                                                      // Values are:
#define NVCPL_API_AA_METHOD_OFF                  0    //   0 = Off  
#define NVCPL_API_AA_METHOD_2X                   1    //   1 = 2X
#define NVCPL_API_AA_METHOD_2XQ                  2    //   2 = 2XQ
#define NVCPL_API_AA_METHOD_4X                   3    //   3 = 4X
#define NVCPL_API_AA_METHOD_4X_GAUSSIAN          4    //   4 = 4X9T
#define NVCPL_API_AA_METHOD_4XS                  5    //   5 = 4XS
#define NVCPL_API_AA_METHOD_6XS                  6    //   6 = 6XS
#define NVCPL_API_AA_METHOD_8XS                  7    //   7 = 8XS
#define NVCPL_API_AA_METHOD_16XS                 8    //   8 = 16XS   

#define NVCPL_API_AGP_LIMIT                      5    // Graphics card GART size.
#define NVCPL_API_FRAME_QUEUE_LIMIT              6    // Graphics card number of frames to prerender.
#define NVCPL_API_NUMBER_OF_GPUS                 7    // Graphics card number of GPUs.
#define NVCPL_API_NUMBER_OF_SLI_GPUS             8    // Graphics card number of SLI GPU clusters available.


#define NVCPL_API_SLI_MULTI_GPU_RENDERING_MODE   9    // Get/Set SLI multi-GPU redering mode. 

// All of constants below should be used as bit-masks for either NvCplGetDataInt or
// NvCplSetDataInt methods dwValue parameter. 

#define NVCPL_API_SLI_ENABLED                    0x10000000  // SLI enabled, when this bit-mask is set. 

#define NVCPL_API_SLI_RENDERING_MODE_AUTOSELECT  0x00000000  // SLI multi-GPU redering mode - Autoselect.

// For current version of ForceWare setting more then one presented below bit-masks would be 
// equals the setting NVCPL_API_SLI_RENDERING_MODE_AUTOSELECT mode.  

#define NVCPL_API_SLI_RENDERING_MODE_AFR         0x00000001  // SLI multi-GPU redering mode - Alternate Frames.
#define NVCPL_API_SLI_RENDERING_MODE_SFR         0x00000002  // SLI multi-GPU redering mode - Split Frame.
#define NVCPL_API_SLI_RENDERING_MODE_SINGLE_GPU  0x00000004  // SLI multi-GPU redering mode - Single GPU.


BOOL __cdecl NvCplGetDataInt( IN DWORD dwSettingIndex, IN DWORD* pdwValue );
typedef BOOL (__cdecl* fNvCplGetDataInt)( IN DWORD dwSettingIndex, IN DWORD* pdwValue );

BOOL __cdecl NvCplSetDataInt( IN DWORD dwSettingIndex, IN DWORD dwValue );
typedef BOOL (__cdecl* fNvCplSetDataInt)( IN DWORD dwSettingIndex, IN DWORD dwValue );


//---------------------------------------------------------------------
// TV Functions
//---------------------------------------------------------------------

// Flags for NVTVOutManageOverscanConfiguration
#define NVCPL_API_OVERSCAN_SHIFT            0x00000010
#define NVCPL_API_UNDERSCAN                 0x00000020
#define NVCPL_API_NATIVEHD                  0x00000080
                  
// Modes for NvSetHDAspect
#define NVAPI_ASPECT_FULLSCREEN             0   // 4:3 aspect ratio
#define NVAPI_ASPECT_LETTERBOX              1   // 4:3 aspect ratio, letterbox
#define NVAPI_ASPECT_WIDESCREEN             2   // 16:9 aspect ratio

// Flags for NvGetDVIAdvancedTimingSupport
#define NVAPI_HDTV_OVER_DVI_EDID_TIMING     0x000000001
#define NVAPI_HDTV_OVER_DVI_CUSTOM_TIMING   0x000000002

#define NVAPI_HDTV_OVER_DVI_TIMING          ( NVAPI_HDTV_OVER_DVI_EDID_TIMING   | \
                                              NVAPI_HDTV_OVER_DVI_CUSTOM_TIMING )

BOOL __cdecl NVTVOutManageOverscanConfiguration( IN DWORD dwTVFormat, IN OUT DWORD *pdwOverscanFlags, IN BOOL bGet );
typedef BOOL (__cdecl* fNVTVOutManageOverscanConfiguration)( IN DWORD dwTVFormat, IN OUT DWORD *pdwOverscanFlags, IN BOOL bGet );

BOOL __cdecl NvSetHDAspect( IN DWORD* pdwAspect );
typedef BOOL (__cdecl* fNvSetHDAspect)( IN DWORD* pdwAspect );

BOOL NVAPIENTRY NvGetRealDVIAdvancedTimingSupport( IN LPCSTR pszUserDisplay, IN DWORD dwTimingType );
typedef BOOL (NVAPIENTRY* fNvGetRealDVIAdvancedTimingSupport)( IN LPCSTR pszUserDisplay, IN DWORD dwTimingType );

BOOL __cdecl NvSetDVDOptimalEnabled( IN BOOL bEnable );
typedef BOOL (__cdecl* fNvSetDVDOptimalEnabled)( IN BOOL bEnable );


//---------------------------------------------------------------------
// OEM Specific Functions
//---------------------------------------------------------------------

// Flags for NvSetOEMConfig
#define NVAPI_ENABLE_SVP_BIT        0x00000001 // Prevent transitions to SVP Video Enhancement chip

BOOL __cdecl NvSetOEMConfig( IN DWORD dwOEMConfig );
typedef BOOL (__cdecl* fNvSetOEMConfig)( IN DWORD dwOEMConfig );


//---------------------------------------------------------------------
// Deprecated Functions 
//---------------------------------------------------------------------
//  These functions remain available for legacy compatibility, but
//  the preferred interfaces indicated below are those officially 
//  supported and recommended for use with newer client code.
//---------------------------------------------------------------------

// Preferred: NvCplGetRealConnectedDevicesString() -- older method has issues with inactive connected displays
BOOL NVAPIENTRY NvCplGetConnectedDevicesString( OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer, IN BOOL bOnlyActive );
typedef BOOL (NVAPIENTRY* fNvCplGetConnectedDevicesString)( OUT LPSTR lpszTextBuffer, IN DWORD cbTextBuffer, IN BOOL bOnlyActive );

// Preferred: NvColorGetGammaRampEx()
BOOL __cdecl NvColorGetGammaRamp( IN LPCSTR pszUserDisplay, OUT GAMMARAMP* pGammaRamp );
typedef BOOL (__cdecl* fNvColorGetGammaRamp)( IN LPCSTR pszUserDisplay, OUT GAMMARAMP* pGammaRamp );

// Preferred: NvColorSetGammaRampEx()
BOOL __cdecl NvColorSetGammaRamp( IN LPCSTR pszUserDisplay, IN DWORD dwUserRotateFlag, IN const GAMMARAMP* pGammaRamp );
typedef BOOL (__cdecl* fNvColorSetGammaRamp)( IN LPCSTR pszUserDisplay, IN DWORD dwUserRotateFlag, IN const GAMMARAMP* pGammaRamp );

// Preferred: NvGetDisplayInfo()
BOOL __cdecl NvCplGetScalingStatus( IN  LPCSTR pszUserDisplay, OUT DWORD* pdwScalingMode );
typedef BOOL (__cdecl* fNvCplGetScalingStatus)( IN  LPCSTR pszUserDisplay, OUT DWORD* pdwScalingMode );

// Preferred: NvGetDisplayInfo() for DFPs the NVDISPLAYINFO1_MAXIMUMSAFEMODE is native resolution
BOOL NVAPIENTRY NvCplGetFlatPanelNativeRes(IN LPCSTR pszUserDisplay, OUT DWORD *pdwHorizontalPixels, OUT DWORD *pdwVerticalPixels);
typedef BOOL (NVAPIENTRY* fNvCplGetFlatPanelNativeRes)(IN LPCSTR pszUserDisplay, OUT DWORD *pdwHorizontalPixels, OUT DWORD *pdwVerticalPixels);

// Preferred: NvGetDisplayInfo()
#define NVGWDS_VIEW_UNKNOWN   -1    // Unknown state
#define NVGWDS_FAILED          0    // Internal error
#define NVGWDS_NOT_FOUND       1    // Unrecognized windows monitor number
#define NVGWDS_UNATTACHED      2    // Graphics card not attached to desktop
#define NVGWDS_ATTACHED        3    // Graphics card attached to desktop but not an NVIDIA device
#define NVGWDS_STANDARD        4    // Graphics card in Single-Display mode (not in DualView)
#define NVGWDS_DUALVIEW        5    // Graphics card in DualView mode (not in Single-Display mode)
#define NVGWDS_CLONE           6    // Graphics card in Clone mode
#define NVGWDS_HSPAN           7    // Graphics card in Horizontal Span mode
#define NVGWDS_VSPAN           8    // Graphics card in Vertical Span mode
int NVAPIENTRY NvGetWindowsDisplayState( IN UINT nWindowsMonitorNumber );
typedef int (NVAPIENTRY* fNvGetWindowsDisplayState)( IN UINT nWindowsMonitorNumber );

// Preferred: NvGetDisplayInfo()
#define NVAPI_TVFORMAT_NTSC_M       0
#define NVAPI_TVFORMAT_NTSC_J       1
#define NVAPI_TVFORMAT_PAL_M        2
#define NVAPI_TVFORMAT_PAL_ABDGHI   3
#define NVAPI_TVFORMAT_PAL_N        4
#define NVAPI_TVFORMAT_PAL_NC       5
#define NVAPI_TVFORMAT_HD576I       8
#define NVAPI_TVFORMAT_HD480I       9
#define NVAPI_TVFORMAT_HD480P       10
#define NVAPI_TVFORMAT_HD576P       11
#define NVAPI_TVFORMAT_HD720P       12
#define NVAPI_TVFORMAT_HD1080I      13
#define NVAPI_TVFORMAT_HD1080P      14
#define NVAPI_TVFORMAT_HD720I       16
#define NVAPI_TVFORMAT_D1           NVAPI_TVFORMAT_HD480I
#define NVAPI_TVFORMAT_D2           NVAPI_TVFORMAT_HD480P
#define NVAPI_TVFORMAT_D3           NVAPI_TVFORMAT_HD1080I
#define NVAPI_TVFORMAT_D4           NVAPI_TVFORMAT_HD720P
#define NVAPI_TVFORMAT_D5           NVAPI_TVFORMAT_HD1080P
BOOL __cdecl NvGetCurrentTVFormat(OUT DWORD* pdwFormat);
typedef BOOL (__cdecl* fNvGetCurrentTVFormat)(OUT DWORD* pdwFormat);

// Preferred: NvGetDisplayInfo()
#define NVAPI_TV_ENCODER_CONNECTOR_UNKNOWN       0x0
#define NVAPI_TV_ENCODER_CONNECTOR_SDTV          0x1
#define NVAPI_TV_ENCODER_CONNECTOR_HDTV          0x2
#define NVAPI_TV_ENCODER_CONNECTOR_HDTV_AND_SDTV 0x3 
BOOL __cdecl NvGetTVConnectedStatus(OUT DWORD* pdwConnected);
typedef BOOL (__cdecl* fNvGetTVConnectedStatus)(OUT DWORD* pdwConnected);

// Preferred: NvGetRealDVIAdvancedTimingSupport() -- older method has issues with inactive connected displays
BOOL __cdecl NvGetDVIAdvancedTimingSupport( IN UINT nWindowsMonitorNumber, IN DWORD dwTimingType );
typedef BOOL (__cdecl* fNvGetDVIAdvancedTimingSupport)( IN UINT nWindowsMonitorNumber, IN DWORD dwTimingType );


///////////////////////////////////////////////////////////////////////

#ifdef REDEFINED_PGAMMARAMP
#   undef GAMMARAMP
#   undef PGAMMARAMP
#   undef REDEFINED_PGAMMARAMP
#endif//REDEFINED_PGAMMARAMP

#ifdef __cplusplus
#   define _cplusplus
} //extern "C" {
#endif

#endif  // __NVPANELAPI_H__
