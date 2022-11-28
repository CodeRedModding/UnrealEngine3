/*=============================================================================
	UnConsoleTools.: Definition of platform agnostic helper functions 
					 implemented in a separate DLL.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Daniel Vogel
=============================================================================*/

#ifdef CONSOLETOOLS_EXPORTS
#define CONSOLETOOLS_API __declspec(dllexport)
#else
#define CONSOLETOOLS_API __declspec(dllimport)
#endif

/**
 * Abstract 2D texture cooker interface.
 * 
 * Expected usage is:
 * Init(...)
 * for( MipLevels )
 * {
 *     Dst = appMalloc( GetMipSize( MipLevel ) );
 *     CookMip( ... )
 * }
 *
 * and repeat.
 */
struct CONSOLETOOLS_API FConsoleTextureCooker
{
	/**
	 * Associates texture parameters with cooker.
	 *
	 * @param UnrealFormat	Unreal pixel format
	 * @param Width			Width of texture (in pixels)
	 * @param Height		Height of texture (in pixels)
	 * @param NumMips		Number of miplevels
	 */
	virtual void Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT NumMips ) = 0;
	/**
	 * Returns the platform specific size of a miplevel.
	 *
	 * @param	Level		Miplevel to query size for
	 * @return	Returns		the size in bytes of Miplevel 'Level'
	 */
	virtual UINT GetMipSize( UINT Level ) = 0;
	/**
	 * Cooks the specified miplevel, and puts the result in Dst which is assumed to
	 * be of at least GetMipSize size.
	 *
	 * @param Level			Miplevel to cook
	 * @param Src			Src pointer
	 * @param Dst			Dst pointer, needs to be of at least GetMipSize size
	 * @param SrcRowPitch	Row pitch of source data
	 */
	virtual void CookMip( UINT Level, void* Src, void* Dst, UINT SrcRowPitch ) = 0;
};

/**
 * Abstract 3D/ volume texture cooker interface.
 * 
 * Expected usage is:
 * Init(...)
 * for( MipLevels )
 * {
 *     Dst = appMalloc( GetMipSize( MipLevel ) );
 *     CookMip( ... )
 * }
 *
 * and repeat.
 */
struct CONSOLETOOLS_API FConsoleVolumeTextureCooker
{
	/**
	 * Associates texture parameters with cooker.
	 *
	 * @param UnrealFormat	Unreal pixel format
	 * @param Width			Width of texture (in pixels)
	 * @param Height		Height of texture (in pixels)
	 * @param Depth			Depth of texture (in pixels)
	 * @param NumMips		Number of miplevels
	 */
	virtual void Init( DWORD UnrealFormat, UINT Width, UINT Height, UINT Depth, UINT NumMips ) = 0;
	/**
	 * Returns the platform specific size of a miplevel.
	 *
	 * @param	Level		Miplevel to query size for
	 * @return	Returns		the size in bytes of Miplevel 'Level'
	 */
	virtual UINT GetMipSize( UINT Level ) = 0;
	/**
	 * Cooks the specified miplevel, and puts the result in Dst which is assumed to
	 * be of at least GetMipSize size.
	 *
	 * @param Level			Miplevel to cook
	 * @param Src			Src pointer
	 * @param Dst			Dst pointer, needs to be of at least GetMipSize size
	 * @param SrcRowPitch	Row pitch of source data
	 * @param SrcSlicePitch Slice pitch of source data
	 */
	virtual void CookMip( UINT Level, void* Src, void* Dst, UINT SrcRowPitch, UINT SrcSlicePitch ) = 0;
};

// Typedef's for easy DLL binding.
typedef FConsoleTextureCooker* (*FuncCreateTextureCooker) (void);
typedef void (*FuncDestroyTextureCooker) (FConsoleTextureCooker*);
typedef FConsoleVolumeTextureCooker* (*FuncCreateVolumeTextureCooker) (void);
typedef void (*FuncDestroyVolumeTextureCooker) (FConsoleVolumeTextureCooker*);
typedef BOOL (*FuncCopyFileToConsole) (char*,char*);

extern "C"
{
	/**
	 * Creates and returns a texture cooker instance.
	 *
	 * @return Returns an instance of the platform specific texture cooker.
	 */
	CONSOLETOOLS_API FConsoleTextureCooker* CreateTextureCooker();
	/**
	 * Destroys an instance.
	 *
	 * @param TextureCooker	Instance to free.
	 */
	CONSOLETOOLS_API void DestroyTextureCooker( FConsoleTextureCooker* TextureCooker );
	/**
	 * Creates and returns a volume texture cooker instance.
	 *
	 * @return Returns an instance of the platform specific volume texture cooker.
	 */
	CONSOLETOOLS_API FConsoleVolumeTextureCooker* CreateVolumeTextureCooker();
	/**
	 * Destroys an instance.
	 *
	 * @param TextureCooker	Instance to free.
	 */
	CONSOLETOOLS_API void DestroyVolumeTextureCooker( FConsoleVolumeTextureCooker* VolumeTextureCooker );
	
	/** 
	 * Copies a file form the local system to the remote console.
	 *
	 * @param Src	Source file path (local machine)
	 * @param Dst	Destination file path (remote machine/ console)
	 * @return TRUE if successful, FALSE otherwise
	 */
	CONSOLETOOLS_API BOOL CopyFileToConsole( char* Src, char* Dst );
}
