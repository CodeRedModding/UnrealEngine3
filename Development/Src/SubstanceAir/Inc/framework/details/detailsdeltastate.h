//! @file detailsdeltastate.h
//! @brief Substance Air Framework Instance Inputs Delta State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111026
//! @copyright Allegorithmic. All rights reserved.
//!
 

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSDELTASTATE_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSDELTASTATE_H

#include "detailsinputstate.h"

#include "SubstanceAirGraph.h"

#include <vector> 

namespace SubstanceAir
{
namespace Details
{

//! @brief Delta of graph instance inputs state 
//! Represents a modification of a graph state: new and previous values of all
//! modified inputs
class DeltaState
{
public:
	//! One input modification
	struct Input
	{
		size_t index;         //!< Input index in GraphState/GraphInstance order
		InputState previous;  //!< Previous input state
		InputState modified;  //!< New input state
		
	};  // struct Input

	//! @brief Vector of input states delta
	typedef std::vector<Input> Inputs;
	
	//! @brief Append modes
	enum AppendMode
	{
		Append_Default  = 0x0,    //!< Merge, use source modified values
		Append_Override = 0x1,    //!< Override previous values
		Append_Reverse  = 0x2     //!< Use source previous values
	};  // enum AppendMode
	
	
	//! @brief Accessor on inputs
	const Inputs& getInputs() const { return mInputs; }
	
	//! @brief Accessor on pointers on ImageInput indexed by mInputs
	const ImageInputPtr& getImageInput(size_t i) const { return mImageInputPtrs.at(i); }
	
	//! @brief Fill a delta state from current state & current instance values
	//! @param graphState The current graph state used to generate delta
	//! @param graphInstance The pushed graph instance (not keeped)
	void fill(
		const GraphState &graphState,
		const FGraphInstance* graphInstance);
		
	//! @brief Append a delta state to current
	//! @param src Source delta state to append
	//! @param mode Append policy flag
	void append(const DeltaState &src,AppendMode mode);

protected:
	//! @brief Order or inputs
	struct InputOrder 
	{ 
		bool operator()(const Input &a,const Input &b) const 
		{ 
			return a.index<b.index;
		} 
	};  // struct InputOrder

	
	//! @brief New and previous values of all modified inputs
	//! Input::index ordered
	Inputs mInputs;

	//! @brief Array of pointers on ImageInput indexed by mInputs
	ImageInputPtrs mImageInputPtrs;
	
	
	//! @brief Record new image pointer if necessary
	//! @param[in,out] inputState The state to record image pointer
	//! @param srcImgPtrs Source image pointers (used if dst is valid image)
	void recordPtr(
		InputState& inputState,
		const ImageInputPtrs &srcImgPtrs);

	//! @brief Return if two input states are equal
	//! @param a The first input state to compare
	//! @param aImgPtrs a image pointers (used if a is valid image)
	//! @param b The second input state to compare
	//! @param bImgPtrs b image pointers (used if b is valid image)
	static UBOOL isEqual(
		const InputState& a,
		const ImageInputPtrs &aImgPtrs,
		const InputState& b,
		const ImageInputPtrs &bImgPtrs);

};  // class DeltaState


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSDELTASTATE_H
