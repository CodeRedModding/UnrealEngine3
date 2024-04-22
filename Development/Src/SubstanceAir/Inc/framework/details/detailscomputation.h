//! @file detailscomputation.h
//! @brief Substance Air Framework Computation action definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111109
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSCOMPUTATION_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSCOMPUTATION_H

#include "SubstanceAirUncopyable.h"
#include "SubstanceAirTypedefs.h"

#pragma pack ( push, 8 )
#include <substance/handle.h>
#pragma pack ( pop )

#include <vector>


namespace SubstanceAir
{
namespace Details
{

class Engine;
struct InputState;

struct ImageInputToken;


//! @brief Substance Engine Computation (render) action
class Computation : Uncopyable
{
public:
	//! @brief Container of indices
	typedef std::vector<UINT> Indices;
	
	
	//! @brief Constructor from engine
	//! @pre Engine is valid and correctly linked
	//! @post Engine render queue is flushed
	Computation(Engine& engine);
	
	//! @brief Set current user data to push w/ I/O
	void setUserData(size_t userData) { mUserData = userData; }
	
	//! @brief Get current user data to push w/ I/O
	size_t getUserData() const { return mUserData; }
	
	//! @brief Push input
	//! @param index SBSBIN index
	//! @param inputState Input type, value and other flags
	//! @param imgToken Image token pointer, used if input image
	//! @return Return if input really pushed
	bool pushInput(
		UINT index,
		const InputState& inputState,
		ImageInputToken* imgToken);
	
	//! @brief Push outputs SBSBIN indices to compute 
	void pushOutputs(const Indices& indices);
	
	//! @brief Run computation
	//! Push hints I/O and run
	void run();
	
protected:

	//! @brief Reference on parent Engine
	Engine &mEngine;
	
	//! @brief Hints outputs indices (SBSBIN indices)
	Indices mHintOutputs;
	
	//! @brief Hints inputs SBSBIN indices (24 MSB bits) and types (8 LSB bits)
	Indices mHintInputs;
	
	//! @brief Current pushed user data
	size_t mUserData;


	//! @brief Get hint input type from mHintInputs value
	static SubstanceInputType getType(UINT v) { return (SubstanceInputType)(v&0xFF); }

	//! @brief Get hint index from mHintInputs value
	static UINT getIndex(UINT v) { return v>>8; }

	//! @brief Get mHintInputs value from type and index
	static UINT getHintInput(SubstanceInputType t,UINT i) { return (i<<8)|(UINT)t; }
	
};  // class Computation


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSCOMPUTATION_H
