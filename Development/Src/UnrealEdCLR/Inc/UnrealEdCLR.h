/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UnrealEdCLR_h__
#define __UnrealEdCLR_h__

#ifdef _MSC_VER
	#pragma once
#endif

// NOTE: This file is used for precompiled header generation (UnrealEdCLR project)

#ifndef __cplusplus_cli
	#error "This file must be compiled as managed code using the /clr compiler option."
#endif

// Engine header files are always compiled unmanaged.  We want to be able to call on engine functions
// from managed code through transparent C++/CLI interop, but we don't want engine code to be managed.
#pragma unmanaged

// Editor/Engine header files
#include "UnrealEd.h"

#include "AssetSelection.h"
#include "BusyCursor.h"
#include "ContentBrowserHost.h"
#include "StartPageHost.h"
#include "EngineClasses.h"
#include "EngineAnimClasses.h"
#include "EngineMeshClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineTextureClasses.h"
#include "PropertyWindow.h"
#include "ScopedTransaction.h"
#include "UnObjectTools.h"
#include "UnPackageTools.h"

#if WITH_FACEFX_STUDIO
#include "../../../External/FaceFX/Studio/Main/Inc/FxStudioApp.h"
#endif

#pragma managed

// STL/CLR: STL-style generic container classes for C++/CLI (cliext namespace)
//   - Requires various including <cliext/...> headers (VC 9.0 or higher)
//#using "Microsoft.VisualC.STLCLR.dll"

// vcclr includes additional interop helper functions (for strings, mostly.)
#include <vcclr.h>

// auto_handle allows CLR objects to be automatically disposed when the reference goes out of scope
#include <msclr\auto_handle.h>

using namespace msclr;

// Interop definitions
#include "InteropShared.h"

// Disable superfluous warning about imported CLR class methods that we might not even be calling from C++/CLI 
#pragma warning(disable : 4564) // method <methodname> of class <classname> defines unsupported default parameter <paramname>

// Disable warning about imported CLR class methods with qualifiers that aren't compatible with C++/CLI
#pragma warning(disable : 4400) // const/volatile qualifiers on this type are not supported

#endif	// __UnrealEdCLR_h__
