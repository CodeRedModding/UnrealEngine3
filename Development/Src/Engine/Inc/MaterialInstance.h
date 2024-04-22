/*=============================================================================
	MaterialInstance.h: MaterialInstance definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _MATERIAL_INSTANCE_H_
#define _MATERIAL_INSTANCE_H_



/**
 * Protects the members of a UMaterialInstanceConstant from re-entrance.
 */
struct FMICReentranceGuard
{
	UMaterialInstance*	Material;

	FMICReentranceGuard(UMaterialInstance* InMaterial):
	Material(InMaterial)
	{
		// NOTE:  we are switching the check to be a log so we can find out which material is causing the assert
		//check(!Material->ReentrantFlag);
		if( Material->ReentrantFlag == 1 )
		{
			warnf( TEXT("InMaterial: %s GameThread: %d RenderThread: %d"), *InMaterial->GetFullName(), IsInGameThread(), IsInRenderingThread() );
			check(!Material->ReentrantFlag);
		}
		if (GUsingMobileRHI == FALSE)
		{
			// Mobile directly accesses the material... 
			check(IsInGameThread() && "GetMaterial() is probably being called from the RenderThread.  You need to cache the value you need on the Proxy object");
		}
		Material->ReentrantFlag = 1;
	}

	~FMICReentranceGuard()
	{
		Material->ReentrantFlag = 0;
	}
};


/**
 * This is a macro that defines a mapping between the game thread UMaterialInstance parameter arrays and the rendering thread
 * FMaterialInstanceResource parameter maps.
 */
#define DEFINE_MATERIALINSTANCE_PARAMETERTYPE_MAPPING(MappingName,ResourceType,InValueType,InParameterType,ArrayName,ResourceArrayName,ParameterToValueCode) \
	class MappingName \
	{ \
	public: \
		typedef ResourceType::InstanceType InstanceType; \
		typedef InValueType ValueType; \
		typedef InParameterType ParameterType; \
		typedef ResourceType::TNamedParameter<ValueType> NamedParameterType; \
		static void GameThread_ClearParameters(const InstanceType* Instance) \
		{ \
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( \
				ClearMIParameters, \
				const InstanceType*,Instance,Instance, \
				{ \
					((ResourceType*)Instance->Resources[0])->ResourceArrayName.Empty(); \
					if(Instance->Resources[1]) \
					{ \
						((ResourceType*)Instance->Resources[1])->ResourceArrayName.Empty(); \
					} \
					if(Instance->Resources[2]) \
					{ \
						((ResourceType*)Instance->Resources[2])->ResourceArrayName.Empty(); \
					} \
				}); \
		} \
		static void RenderThread_UpdateParameter( ResourceType* Resource, const FName Name, const ValueType& Value ) \
		{ \
			const INT ParameterCount = Resource->ResourceArrayName.Num(); \
			for ( INT ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex ) \
			{ \
				NamedParameterType& Parameter = Resource->ResourceArrayName( ParameterIndex ); \
				if ( Parameter.Name == Name ) \
				{ \
					Parameter.Value = Value; \
					return; \
				} \
			} \
			NamedParameterType NewParameter; \
			NewParameter.Name = Name; \
			NewParameter.Value = Value; \
			Resource->ResourceArrayName.AddItem( NewParameter ); \
		} \
		static void GameThread_UpdateParameter(const InstanceType* Instance,const ParameterType& Parameter) \
		{ \
			ValueType Value; \
			UBOOL bHasValue = TRUE; \
			ParameterToValueCode; \
			if(bHasValue) \
			{ \
				ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( \
					SetMIParameterValue, \
					const InstanceType*,Instance,Instance, \
					FName,ParameterName,Parameter.ParameterName, \
					ValueType,Value,Value, \
					{ \
						RenderThread_UpdateParameter( ((ResourceType*)Instance->Resources[0]), ParameterName, Value ); \
						if(Instance->Resources[1]) \
						{ \
							RenderThread_UpdateParameter( ((ResourceType*)Instance->Resources[1]), ParameterName, Value ); \
						} \
						if(Instance->Resources[2]) \
						{ \
							RenderThread_UpdateParameter( ((ResourceType*)Instance->Resources[2]), ParameterName, Value ); \
						} \
					}); \
			} \
		} \
		static const TArray<ParameterType>& GetParameterArray(const InstanceType* Instance) \
		{ \
			return Instance->ArrayName; \
		} \
		static ParameterType* FindParameterByName(InstanceType* Instance,FName ParameterName) \
		{ \
			for (INT ValueIndex = 0;ValueIndex < Instance->ArrayName.Num();ValueIndex++) \
			{ \
				if (Instance->ArrayName(ValueIndex).ParameterName == ParameterName) \
				{ \
					return &Instance->ArrayName(ValueIndex); \
				} \
			} \
			return NULL; \
		} \
		FORCEINLINE static const ValueType* FindResourceParameterByName(const ResourceType* Resource,const FName ParameterName) \
		{ \
			const INT ParameterCount = Resource->ResourceArrayName.Num(); \
			for ( INT ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex ) \
			{ \
				const NamedParameterType& Parameter = Resource->ResourceArrayName( ParameterIndex ); \
				if ( Parameter.Name == ParameterName ) \
				{ \
					return &Parameter.Value; \
				} \
			} \
			return NULL; \
		} \
	};

/**
* The resource used to render a UMaterialInstance.
*/
class FMaterialInstanceResource: public FMaterialRenderProxy, public FDeferredCleanupInterface
{
public:

	/** Material instances store pairs of names and values in arrays to look up parameters at run time. */
	template <typename ValueType>
	struct TNamedParameter
	{
		FName Name;
		ValueType Value;
	};

	/** Initialization constructor. */
	FMaterialInstanceResource(UMaterialInstance* InOwner,UBOOL bInSelected,UBOOL bInHovered);

	/** Sets the material instance's parent. */
	void GameThread_SetParent(UMaterialInterface* InParent);

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial() const;

	virtual FLOAT GetDistanceFieldPenumbraScale() const { return DistanceFieldPenumbraScale; }

	virtual UBOOL IsSelected() const { return bSelected; }
	virtual UBOOL IsHovered() const { return bHovered; }

	/** Called from the game thread to update DistanceFieldPenumbraScale. */
	void UpdateDistanceFieldPenumbraScale(FLOAT NewDistanceFieldPenumbraScale);

#if WITH_MOBILE_RHI
	/**
	 * For UMaterials, this will return the flattened texture for platforms that don't 
	 * have full material support
	 *
	 * @return the FTexture object that represents the flattened texture for this material (can be NULL)
	 */
	virtual class FTexture* GetMobileTexture(const INT MobileTextureUnit) const;

	virtual void FillMobileMaterialVertexParams (FMobileMaterialVertexParams& OutVertexParams) const;
	virtual void FillMobileMaterialPixelParams (FMobileMaterialPixelParams& OutPixelParams) const;
#endif

protected:

	/** The parent of the material instance. */
	UMaterialInterface* Parent;

	/** The UMaterialInstance which owns this resource. */
	UMaterialInstance* Owner;

// 	UMaterialInstance* TextureHoldingMaterial;

	FLOAT DistanceFieldPenumbraScale;

	/** Whether this resource represents the selected version of the material instance. */
	UBOOL bSelected;

	/** Whether this resource represents the hovered version of the material instance. */
	UBOOL bHovered;

	/** The game thread accessible parent of the material instance. */
	UMaterialInterface* GameThreadParent;
};

namespace
{
	/**
	* This function takes a array of parameter structs and attempts to establish a reference to the expression object each parameter represents.
	* If a reference exists, the function checks to see if the parameter has been renamed.
	*
	* @param Parameters		Array of parameters to operate on.
	* @param ParentMaterial	Parent material to search in for expressions.
	*
	* @return Returns whether or not any of the parameters was changed.
	*/
	template <typename ParameterType, typename ExpressionType>
	UBOOL UpdateParameterSet(TArray<ParameterType> &Parameters, UMaterial* ParentMaterial)
	{
		UBOOL bChanged = FALSE;

		// Loop through all of the parameters and try to either establish a reference to the 
		// expression the parameter represents, or check to see if the parameter's name has changed.
		for(INT ParameterIdx=0; ParameterIdx<Parameters.Num(); ParameterIdx++)
		{
			UBOOL bTryToFindByName = TRUE;

			ParameterType &Parameter = Parameters(ParameterIdx);

			if(Parameter.ExpressionGUID.IsValid())
			{
				ExpressionType* Expression = ParentMaterial->FindExpressionByGUID<ExpressionType>(Parameter.ExpressionGUID);

				// Check to see if the parameter name was changed.
				if(Expression)
				{
					bTryToFindByName = FALSE;

					if(Parameter.ParameterName != Expression->ParameterName)
					{
						Parameter.ParameterName = Expression->ParameterName;
						bChanged = TRUE;
					}
				}
			}

			// No reference to the material expression exists, so try to find one in the material expression's array if we are in the editor.
			if(bTryToFindByName && GIsEditor && !GIsGame)
			{
				for(INT ExpressionIndex = 0;ExpressionIndex < ParentMaterial->Expressions.Num();ExpressionIndex++)
				{
					ExpressionType* ParameterExpression = Cast<ExpressionType>(ParentMaterial->Expressions(ExpressionIndex));

					if(ParameterExpression && ParameterExpression->ParameterName == Parameter.ParameterName)
					{
						Parameter.ExpressionGUID = ParameterExpression->ExpressionGUID;
						bChanged = TRUE;
						break;
					}
				}
			}
		}

		return bChanged;
	}
}



#endif // _MATERIAL_INSTANCE_H_


