//! @file constants.h
//! @brief Few default values used in the framework
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_FRAMEWORK_CONSTANTS_H
#define _SUBSTANCE_AIR_FRAMEWORK_CONSTANTS_H

namespace SubstanceAir
{
namespace Constants
{

//! @brief Default memory budget used when rendering actively
const unsigned int RenderingMemoryBudget = 512*1024*1024;

//! @brief Default memory budget used when idle (no rendering, but keeping cache)
const unsigned int IdleMemoryBudget = 256*1024*1024;

}
}

#endif //_SUBSTANCE_AIR_FRAMEWORK_CONSTANTS_H
