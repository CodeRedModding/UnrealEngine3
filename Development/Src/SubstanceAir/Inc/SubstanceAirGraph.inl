//! @file SubstanceAirGraph.inl
//! @brief Substance Air graph inline implementation
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

namespace SubstanceAir
{

template< typename T > int_t FGraphInstance::UpdateInputHelper(
	input_inst_t* InputInst, 
	input_desc_t* InputDesc, 
	const TArray< T > & InValue)
{
	int_t ModifiedOuputs = 0;

	if (InputInst->IsNumerical())
	{
		num_input_inst_t* NumInputInst = (num_input_inst_t*) InputInst;

		NumInputInst->SetValue< T >(InValue);

		for (UINT Idx=0 ; Idx<InputDesc->AlteredOutputUids.size() ; ++Idx)
		{
			output_inst_t* Output = this->GetOutput(InputDesc->AlteredOutputUids(Idx));

			if (Output && Output->bIsEnabled)
			{
				++ModifiedOuputs;
				Output->flagAsDirty();
			}
		}
	}

	return ModifiedOuputs;
}


template< typename T > int_t FGraphInstance::UpdateInput(
	const uint_t& Uid,
	const TArray< T >& InValue)
{
	graph_desc_t* ParentGraph = Outputs[0].GetParentGraph();
	List<input_desc_ptr>::TIterator 
		ItIn(ParentGraph->InputDescs.itfront());

	int_t ModifiedOuputs = 0;

	for ( ; ItIn ; ++ItIn)
	{
		if ((*ItIn)->Uid == Uid)
		{
			input_desc_t* InputDesc = (*ItIn).get();

			// instances and descs should be stored in the same order
			input_inst_t* InputInst = Inputs[ItIn.GetIndex()].get();
			check(InputDesc->Uid == InputInst->Uid);

			ModifiedOuputs += UpdateInputHelper(InputInst, InputDesc, InValue);
		}
	}

	return ModifiedOuputs;
}


template< typename T > int_t FGraphInstance::UpdateInput(
	const FString& ParameterName,
	const TArray< T > & InValue)
{
	graph_desc_t* ParentGraph = Outputs[0].GetParentGraph();
	List<input_desc_ptr>::TIterator 
		ItIn(ParentGraph->InputDescs.itfront());

	int_t ModifiedOuputs = 0;

	for ( ; ItIn ; ++ItIn)
	{
		if ((*ItIn)->Identifier == ParameterName)
		{
			input_desc_t* InputDesc = (*ItIn).get();
			input_inst_t* InputInst = Inputs[ItIn.GetIndex()].get();

			// instances and descs should be stored in the same order
			check(InputDesc->Uid == InputInst->Uid);

			ModifiedOuputs += UpdateInputHelper(InputInst, InputDesc, InValue);
		}
	}

	return ModifiedOuputs;
}

} // namespace SubstanceAir
