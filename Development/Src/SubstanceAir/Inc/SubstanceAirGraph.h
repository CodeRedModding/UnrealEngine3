//! @file SubstanceAirGraph.h
//! @brief Substance Air graph definition
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_GRAPH_H
#define _SUBSTANCE_AIR_GRAPH_H

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirHelpers.h"

#include "framework/details/detailsstates.h"

class USubstanceAirGraphInstance;

namespace SubstanceAir
{
	struct FGraphDesc
	{
		FGraphDesc():PackageUrl(TEXT("")), Label(TEXT("")), Parent(0){}
		~FGraphDesc();

		//! @brief Create a default instance of the Graph
		graph_inst_t* Instantiate(
			USubstanceAirGraphInstance* Outer,
			UBOOL bCreateOutputs=TRUE,
			UBOOL bSubscribeInstance=TRUE);

		//! @brief Get the default hash used to differentiate instances' group
		input_hash_t getDefaultHeavyInputHash() const;

		//! @brief Return the output desc with the given Substance Uid
		output_desc_t* GetOutputDesc(const uint_t Uid);

		//! @brief Return the int desc with the given Substance Uid
		input_desc_ptr GetInputDesc(const uint_t Uid);

		//! @brief Instances are registered after loading or instancing
		//! @note Their UID has been registered during creation (@see InstanceUids)
		void Subscribe(graph_inst_t* Inst);

		//! @brief Instances are registered when unloaded or destroyed
		//! @note Their UID still has to be removed after that (@see InstanceUids)
		void UnSubscribe(graph_inst_t* Inst);

		FString PackageUrl;
		FString Label;
		FString Description;

		//! @brief The outputs of the graph
		List<output_desc_t> OutputDescs;

		//! @brief The inputs of the graph
		List<input_desc_ptr> InputDescs;

		//! @brief Instances of graph currently loaded
		//! @note There is at least one, the instances are owned by the USubstanceAirGraphInstance
		List<graph_inst_t*> LoadedInstances;

		//! @brief UIDs of the all existing instances
		List<guid_t> InstanceUids;

		//! @brief the Parent package
		package_t* Parent;

	private:
		//disable copy constructor
		FGraphDesc(const FGraphDesc&);
		FGraphDesc& operator = (const FGraphDesc&);
	};


	struct FGraphInstance
	{
		FGraphInstance():InstanceGuid(0,0,0,0), Desc(NULL), ParentInstance(NULL),bIsFreezed(FALSE), bIsBaked(FALSE), bHasPendingImageInputRendering(FALSE){}

		FGraphInstance(FGraphDesc*, USubstanceAirGraphInstance* Outer);

		~FGraphInstance();

		//! @brief Get the hash used to differentiate instances' group
		//! Heavy-duty tweaks invalidate too much tweak and we need
		//! to handle some instances in separate groups, hashing
		//! the heavy-duty inputs is a good starting-point
		input_hash_t getHeavyInputHash() const;

		//! @brief Update the input with that name with the given values
		//! @return Number of modified outputs
		template< typename T > int_t UpdateInput(
			const FString& ParameterName,
			const TArray< T >& Value);

		//! @return Number of modified outputs
		template< typename T > int_t UpdateInput(
			const uint_t& Uid,
			const TArray< T >& Value);

		//! @brief Update the input with that name with the given values
		//! @return Number of modified outputs
		int_t UpdateInput(
			const uint_t& Uid,
			class UObject* Value);

		//! @return Number of modified outputs
		int_t UpdateInput(
			const FString& ParameterName,
			class UObject* Value);

		//! @brief Return the instance of the input with the given UID
		input_inst_t* GetInput(const uint_t Uid);

		//! @brief Return the instance of the input with the given UID
		input_inst_t* GetInput(const FString& ParameterName);

		//! @brief Return the instance of the output with the given UID
		output_inst_t* GetOutput(const uint_t Uid);

		void plugState(Details::States*);         //!< Internal use only
		void unplugState(Details::States*);       //!< Internal use only

		//! @brief Array of output instances
		SubstanceAir::List<output_inst_t> Outputs;

		//! @brief Array of input instances
		SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>> Inputs;

		//! @brief GUID of this instance
		guid_t InstanceGuid;

		//! @brief UID of the parent Desc
		FString ParentUrl;

		//! @brief the graph's description
		graph_desc_t* Desc;

		//! @brief the UObject based interface object
		USubstanceAirGraphInstance* ParentInstance;

		//! @brief The user can disable runtime modifications of the output
		BITFIELD	bIsFreezed:1;

		//! @brief The user can disable outputs removal during cooking
		BITFIELD	bIsBaked:1;

		//! @brief Does the graph need some SubstanceTexture2D to be rendered (seekfree)
		BITFIELD	bHasPendingImageInputRendering:1;

	protected:
		typedef std::vector<Details::States*> States_t;

		States_t States;

		template< typename T > int_t UpdateInputHelper(
			input_inst_t* InputInst,
			input_desc_t* InputDesc,
			const TArray< T > & InValue );
	};

} // namespace SubstanceAir

#include "SubstanceAirGraph.inl"

#endif //_SUBSTANCE_AIR_GRAPH_H
