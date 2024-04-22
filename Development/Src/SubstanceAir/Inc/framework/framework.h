//! @file framework.h
//! @brief Substance Air Framework includes
//! @author Gonzalez Antoine - Allegorithmic
//! @date 20100429
//! @copyright Allegorithmic. All rights reserved.

/** @mainpage Substance - Framework API Documentation

	@section intro_sec Introduction

	The Substance Framework API allows the generation of textures produced by
	the Substance authoring tools. It uses the Substance Engine and Substance
	Linker components. This Framework is a C++ wrapper to be used in a context
	where interactive parameter tweaking is involved. It simplifies the loading
	of Substance assets, the management of tweaks cache and of render instances.

	@section start_sec Getting started with the Substance Framework

	The include entry point is framework.h header.

	The most important header files are package.h and renderer.h. See
	the documentation of these headers for a start.

	@section compile_howto_sec Compilation HOWTO

	To compile the Substance Framework API, and any source that includes this
	API; the following components include directories must be in the include
	path:
	 - Substance Engine
	 - Substance Linker
	 - TinyXML

	At link time, the Substance Framework API uses these dependencies:
	 - Substance Engine library, CPU version: (substance_sse2_blend)
	 - Substance Linker library (substance_linker)
	 - TinyXML

	@section tinyxml_sec TinyXML

	To use TinyXML with custom namespace, use the 'TINYXML_CUSTOM_NAMESPACE'
	preprocessor define (e.g. 'MyTinyXmlNamespace::').
 */

#ifndef _SUBSTANCE_AIR_FRAMEWORK_H
#define _SUBSTANCE_AIR_FRAMEWORK_H

#include "renderer.h"
#include "stacking.h"

#endif // _SUBSTANCE_AIR_FRAMEWORK_H
