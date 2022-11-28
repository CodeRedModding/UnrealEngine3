/*=============================================================================
	D3DResourceHelper.h: Unreal Direct3D resource definition helper classes.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Split off into separate file by Daniel Vogel
=============================================================================*/

/**
 * A wrapper around D3D resources to safely handle reference counting and 
 * automatic release of resources.
 */
template<class T> struct TD3DRef
{
	T*	Handle;

	// Constructor/destructor.

	TD3DRef(T* InHandle = NULL):
		Handle(InHandle)
	{
		if(Handle)
			Handle->AddRef();
	}

	TD3DRef(const TD3DRef<T>& Copy)
	{
		Handle = Copy.Handle;

		if(Handle)
			Handle->AddRef();
	}

	~TD3DRef()
	{
		if(Handle)
			Handle->Release();
	}

	// Assignment operator.

	void operator=(T* InHandle)
	{
		if(Handle)
			Handle->Release();

		Handle = InHandle;

		if(Handle)
			Handle->AddRef();
	}

	void operator=(const TD3DRef<T>& Other)
	{
		if(Handle)
			Handle->Release();

		Handle = Other.Handle;

		if(Handle)
			Handle->AddRef();
	}

	// Access operators.

	typedef	T*	PtrT;
	operator PtrT() const
	{
		return Handle;
	}

	T* operator->() const
	{
#ifndef XBOX
		//@todo xenon: get the code to compile without any checks.
		checkSlow(Handle);
#endif
		return Handle;
	}
};

