//! @file detailsinputimagetoken.h
//! @brief Substance Air Framework ImageInputToken definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111122
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSINPUTIMAGETOKEN_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSINPUTIMAGETOKEN_H

#include "detailssync.h"

#pragma pack ( push, 8 )
#include <substance/textureinput.h>
#pragma pack ( pop )

namespace SubstanceAir
{
namespace Details
{

//! @brief Definition of the class used for scoped image access sync
//! Contains texture input description
//! Use a simple mutex (to replace w/ R/W mutex)
struct ImageInputToken : public SubstanceTextureInput
{
	//! @brief Lock access
	void lock() { mMutex.lock(); }
	
	//! @brief Unlock access
	void unlock()  { mMutex.unlock(); }

protected:
	Sync::mutex mMutex;           //!< Main mutex
	
};  // class InputImageToken


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSINPUTIMAGETOKEN_H
