//! @file callbacks.h
//! @brief Substance Air callbacks structure definition
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_FRAMEWORK_CALLBACKS_H
#define _SUBSTANCE_AIR_FRAMEWORK_CALLBACKS_H

#include "SubstanceAirTypedefs.h"


namespace SubstanceAir
{

//! @brief Abstract base structure of user callbacks
//! Inherit from this base struct and overload the virtual methods to be 
//! notified by framework callbacks.
//!
//! Use concrete callbacks instance in Renderer::setCallbacks().
//! @warning Callback methods can be called from a different thread, not 
//!		necessary the user thread (that call Renderer::run()).
struct Callbacks
{
	//! @brief Destructor
	virtual ~Callbacks() {}

	//! @brief Output computed callback (render result available)
	//! @param runUid The UID of the corresponding computation (returned by 
	//! 	Renderer::run()).
	//! @brief graphInstance Pointer on output parent graph instance
	//! @param outputInstance Pointer on computed output instance w/ new render
	//!		result just available (use OutputInstance::grabResult() to grab it).
	//! @note This callback must be implemented in concrete Callbacks structures.
	//!
	//! Called each time an new render result is just available.
	virtual void outputComputed(
		UINT runUid,
		const FGraphInstance* graphInstance,
		FOutputInstance* outputInstance) = 0;
};

} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_FRAMEWORK_CALLBACKS_H
