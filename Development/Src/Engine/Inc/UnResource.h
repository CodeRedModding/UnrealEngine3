/*=============================================================================
	UnResource.h: Resource management definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

//
//	Forward definitions.
//

struct FResource;
struct FTextureMipRequest;
struct FTextureBase;

#undef UpdateResource // Get rid of WinBase.h remapping of UpdateResource -> UpdateResource<A/W>

//
//	FResourceClient
//

struct FResourceClient
{
	// FreeResource

	virtual void FreeResource(FResource* Resource) = 0;

	// UpdateResource

	virtual void UpdateResource(FResource* Resource) = 0;

	/**
	 * Request an increase or decrease in the amount of miplevels a texture uses.
	 *
	 * @param	Texture				texture to adjust
	 * @param	RequestedMips		miplevels the texture should have
	 * @param	TextureMipRequest	NULL if a decrease is requested, otherwise pointer to information 
	 *								about request that needs to be filled in.
	 */
	virtual void RequestMips( FTextureBase* Texture, UINT RequestedMips, FTextureMipRequest* TextureMipRequest ) = 0;

	/**
	 * Finalizes a mip request. This gets called after all requested data has finished loading.	
	 *
	 * @param	TextureMipRequest	Mip request that is being finalized
	 */
	virtual void FinalizeMipRequest( FTextureMipRequest* TextureMipRequest ) = 0;
};

//
//	FResourceManager
//

struct FResourceManager
{
private:

	TArray<FResourceClient*>	Clients;
	INT							NextResourceIndex;

public:

	// Constructor.

	FResourceManager();

	// RegisterClient

	void RegisterClient(FResourceClient* Client);

	// DeregisterClient

	void DeregisterClient(FResourceClient* Client);

	// AllocateResource - Sets the resource's ResourceIndex to a unique index.

	void AllocateResource(FResource* Resource);

	// FreeResource

	void FreeResource(FResource* Resource);

	// UpdateResource

	void UpdateResource(FResource* Resource);

	/**
	 * Request an increase or decrease in the amount of miplevels a texture uses.
	 *
	 * @param	Texture				texture to adjust
	 * @param	RequestedMips		miplevels the texture should have
	 * @return	NULL if a decrease is requested, pointer to information about request otherwise (NOTE: pointer gets freed in FinalizeMipRequest)
	 */
	FTextureMipRequest* RequestMips( FTextureBase* Texture, UINT RequestedMips );

	/**
	 * Finalizes a mip request. This gets called after all requested data has finished loading.	
	 *
	 * @param	TextureMipRequest	Mip request that is being finalized.
	 */
	void FinalizeMipRequest( FTextureMipRequest* TextureMipRequest );
};

extern FResourceManager* GResourceManager;

//
//	FResourceType
//

struct FResourceType
{
	const TCHAR*	Name;
	FResourceType*	Super;

	// Constructor.

	FResourceType(const TCHAR* InName,FResourceType* InSuper):
		Name(InName),
		Super(InSuper)
	{
	}

	// IsA

	FORCEINLINE UBOOL IsA(FResourceType& Type)
	{
		FResourceType* CurrentSuper = this;
		while(CurrentSuper)
		{
			if(CurrentSuper == &Type)
				return 1;
			CurrentSuper = CurrentSuper->Super;
		};
		return 0;
	}
};

#define DECLARE_RESOURCE_TYPE(TypeName,SuperType) \
	typedef SuperType SuperResource; \
	static FResourceType StaticType; \
	virtual FResourceType* Type() { return &StaticType; }

#define IMPLEMENT_RESOURCE_TYPE(TypeName) \
	FResourceType TypeName::StaticType(TEXT(#TypeName),&TypeName::SuperResource::StaticType);

//
//	FResource
//

struct FResource
{
	static FResourceType	StaticType;

	INT		ResourceIndex;
	UBOOL	Dynamic;

	// Constructor.

	FResource()
	:	ResourceIndex(INDEX_NONE), 
		Dynamic(0)
	{
		GResourceManager->AllocateResource(this);
	}

	// Destructor.

	~FResource()
	{
		if(ResourceIndex != INDEX_NONE)
			GResourceManager->FreeResource(this);
	}

	// FResource interface.

	virtual FResourceType* Type() { return &StaticType; }
	virtual FString DescribeResource() { return TEXT(""); }
	virtual FGuid GetPersistentId() { return FGuid(0,0,0,0); }
	
	/**
 	 *	Returns whether this resource can be streamed in. The default behaviour is to not allow streaming 
	 * 	and have derived classes explicitely enable it.
	 *
	 *	@return TRUE if resource can be streamed, FALSE otherwise.
	 */
	virtual UBOOL CanBeStreamed() { return 0; }
};


/**
 * Safely casts a FResource to a specific type.
 *
 * @param 	template	type of result
 * @param	Src			resource to be cast
 * @return	Src if resource is of the right type, NULL otherwise
 */
template< class T > T* CastResource( FResource* Src )
{
	return Src && Src->Type()->IsA(T::StaticType) ? (T*)Src : NULL;
}

/**
 * Casts a FResource to a specific type and asserts if Src is not a subclass of the requested type.
 *
 * @param 	template	type of result
 * @param	Src			resource to be cast
 * @return	Src cast to the destination type
 */
template< class T, class U > T* CastResourceChecked( U* Src )
{
	if( !Src || !Src->Type()->IsA(T::StaticType) )
		appErrorf( TEXT("Cast of %s to %s failed"), Src ? *Src->DescribeResource() : TEXT("NULL"), T::StaticType.Name );
	return (T*)Src;
}

/**
 * Returns a hash value for FResource pointers using the unique ResourceIndex
 *
 * @param	Resource to create the hash value for
 * @return	Hash value using the unique ResourceIndex
 */
inline DWORD GetTypeHash( FResource* Resource ) 
{ 
	return Resource->ResourceIndex;
}
