//! @file detailsoutputsfilter.h
//! @brief Substance Air Framework Outputs filtering structure definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111107
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSOUTPUTSFILTER_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSOUTPUTSFILTER_H

#include "SubstanceAirTypedefs.h"

#include "framework/details/detailsrenderjob.h"

#include <vector> 
#include <map> 


namespace SubstanceAir
{
namespace Details
{


//! @brief Contains a list of outputs to filter per Graph Instance
struct OutputsFilter
{
	//! @brief List of outputs to filter container
	//! GraphState/GraphInstance order indices.
	typedef std::vector<UINT> Outputs;
	
	//! @brief List of outputs to filter per Instance UID container
	typedef std::map<guid_t,Outputs, guid_t_comp> Instances;
	
	//! @brief Constructor from RenderJob contains the list of outputs to filter
	OutputsFilter(const RenderJob& src);
	
	//! @brief List of outputs to filter per Instance UID
	//! Outputs are GraphState/GraphInstance order indices.
	Instances instances;
	
};  // struct OutputsFilter


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSOUTPUTSFILTER_H
