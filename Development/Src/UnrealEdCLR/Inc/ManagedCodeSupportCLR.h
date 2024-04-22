/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ManagedCodeSupportCLR_h__
#define __ManagedCodeSupportCLR_h__

#ifdef _MSC_VER
	#pragma once
#endif


#include "UnrealEdCLR.h"

using namespace System;
using namespace System::Windows;
using namespace System::Windows::Media;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Collections::Specialized;
using namespace System::Collections::Generic;


/**
 * MNativePointer
 *
 * Implements smart-pointer semantics for containing native types within managed objects
 */
template< typename T >
ref struct MNativePointer
{
	MNativePointer()
		: NativePointer( NULL )
	{
	}

	MNativePointer( T* InNativePointer )
		: NativePointer( InNativePointer )
	{
	}

	MNativePointer( MNativePointer< T >% InScopedNativePointer )
		: NativePointer( InScopedNativePointer.Release() )
	{
	}

	T* operator->()
	{
		check( NativePointer != NULL );
		return NativePointer;
	}

	operator bool()
	{
		return ( NativePointer != NULL );
	}

	bool IsValid()
	{
		return ( NativePointer != NULL );;
	}

	T* Get()
	{
		return NativePointer;
	}

	T* Release()
	{
		T* ReleasedNativePointer = NativePointer;
		NativePointer = NULL;
		return ReleasedNativePointer;
	}

	void Reset()
	{
		Reset( NULL );
	}

	virtual void Reset( T* InNativePointer )
	{
		NativePointer = InNativePointer;
	}


protected:

	/** Actual pointer to the native type we're holding */
	T* NativePointer;

};




/**
 * MScopedNativePointer
 *
 * Like MNativePointer except takes ownership of the object and deletes it when out of scope
 */
template< typename T >
ref struct MScopedNativePointer
	: public MNativePointer< T >
{

	~MScopedNativePointer()
	{
		delete NativePointer;
		NativePointer = NULL;
	}

	!MScopedNativePointer()
	{
		//check( NativePointer == NULL );
		delete NativePointer;
	}

	virtual void Reset( T* InNativePointer ) override
	{
		if( InNativePointer != NativePointer )
		{
			delete NativePointer;
			NativePointer = InNativePointer;
		}
	}


};

/**
 * MEnumerableTArrayWrapper - wrapper for Unreal TArray to provide an IEnumerable interface so the data it can be bound to a WPF ItemsSource.
 * @templateparam ElementWrapperType - managed class taking a reference to the TArray element end exposing the variables as properties
 * @templateparam ElementType - native struct stored in the TArray.
 */
template<class ElementWrapperType, class ElementType>
ref class MEnumerableTArrayWrapper : System::Collections::IEnumerable, INotifyCollectionChanged, INotifyPropertyChanged
{
public:
	virtual event NotifyCollectionChangedEventHandler^ CollectionChanged;
	virtual event PropertyChangedEventHandler^ PropertyChanged;
	

	MEnumerableTArrayWrapper(TArray<ElementType>* InArrayPtr)
	:	ArrayPtr(InArrayPtr)
	{}

	virtual System::Collections::IEnumerator^ GetEnumerator() sealed = System::Collections::IEnumerable::GetEnumerator
	{ 
		return gcnew MTArrayEnumerator(this, ArrayPtr); 
	}

	void NotifyChanged()
	{
		CollectionChanged(this, gcnew NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction::Reset));
	}

	void OnItemPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		// pass the notification on
		PropertyChanged(Owner, Args);
	}


private:
	ref struct MTArrayEnumerator : System::Collections::IEnumerator
	{
		TArray<ElementType>* ArrayPtr;
		INT Pos;

	public:
		MTArrayEnumerator( MEnumerableTArrayWrapper^ InParent, TArray<ElementType>* InArrayPtr )
		:	Parent(InParent)
		,	ArrayPtr(InArrayPtr)
		,	Pos(-1)
		{		
		}

		virtual bool MoveNext()
		{
			if( Pos < ArrayPtr->Num()-1 )
			{
				Pos++;
				return true;
			}
			return false;
		}

		virtual void Reset()
		{
			Pos = -1;
		}

		property Object^ Current
		{ 
			virtual Object^ get() sealed = System::Collections::IEnumerator::Current::get
			{ 
				ElementWrapperType^ Wrapper = gcnew ElementWrapperType(Pos, (*ArrayPtr)(Pos));
				Wrapper->PropertyChanged += gcnew PropertyChangedEventHandler( Parent, &MEnumerableTArrayWrapper::OnItemPropertyChanged );
				return Wrapper;
			} 
		};

		MEnumerableTArrayWrapper^ Parent;
	};

	TArray<ElementType>* ArrayPtr;
};


namespace CLRTools
{

	/** Creates a WPF window as a child of a Win32 window from a visual resource */
	Interop::HwndSource^ CreateWPFWindowFromVisual( HWND ParentWindow,
													Visual^ InWPFVisual,
													String^ WindowTitle,
													int X,
													int Y,
													int Width,
													int Height,
													int WindowStyle,
													int ExtendedWindowStyle,
													int WindowClassStyle,
													bool bUsesPerPixelOpacity,
													bool bSizeToContent );

	/** Creates a WPF window as a child of a Win32 window from XAML source on disk */
	Interop::HwndSource^ CreateWPFWindowFromXaml( HWND ParentWindow,
												  String^ XamlFileName,
												  String^ WindowTitle,
												  int X,
												  int Y,
												  int Width,
												  int Height,
												  int WindowStyle,
												  int ExtendedWindowStyle,
												  int WindowClassStyle,
												  bool bUsesPerPixelOpacity,
												  bool bSizeToContent );

	/** 
	 * Create a Visual from xaml 
	 */
	Visual^ CreateVisualFromXaml (String^ XamlFileName);


	/** Converts a CLR String directly to an FName */
	FName ToFName( String^ InCLRString );
	
	/** Converts a CLR String to an FString */
	FString ToFString( String^ InCLRString );

	/** Converts an FString to a CLR String */
	String^ ToString( const FString& InFString );

	/** Converts an FName to a CLRString */
	String^ FNameToString( const FName& InFName );

	/** Converts an FString array to a CLR String array */
	List< String^ >^ ToStringArray( const TArray< FString >& InFStrings );

	/** Converts a CLR String array to an FString array */
	void ToFStringArray( Generic::ICollection< String^ >^ InStrings, TArray< FString >& OutFStrings );

	/** Converts an FColor to a CLR Color */
	Color ToColor(const FColor& InFColor );

	/** Localizes the specified string and returns the result */
	String^ LocalizeString( String^ UnlocalizedString, String^ StrParam0 = nullptr, String^ StrParam1 = nullptr, String^ StrParam2 = nullptr );

	/** Recursively localizes all string-based content in the specified control and it's children */
	void LocalizeContentRecursively( Visual^ Visual );

	/** Writes the specified text to the warning log */
	void LogWarningMessage( String^ Message );

	/** 
	 * Writes the specified text to the warning log 
	 *
	 * @param	CategoryName	The name of the category for the log message.
	 * @param	Message			The message to log.
	 */
	void LogWarningMessage( EName CategoryName, String^ Message );

	/**
	 * Utility method for extracting the package name of an asset
	 *
	 * @param	AssetPathName
	 */
	FString ExtractPackageName( const FString& AssetPathName );

	/**
	 * Wrapper for determining whether a package should be added to the packages tree-view.
	 *
	 * @param	Pkg		the package to check
	 *
	 * @return	FALSE if the package should be ignored.
	 */
	UBOOL IsPackageValidForTree( UPackage* Pkg );

	/**
	 * Wrapper for determining whether an asset is a map package or contained in a map package
	 *
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 *
	 * @return	TRUE if the specified asset is a map package or contained in a map package
	 */
	bool IsMapPackageAsset( FString AssetPathName );
	bool IsMapPackageAsset( String ^AssetPathName );

	/**
	 * Wrapper for determining whether an asset is eligible to be loaded on its own.
	 * 
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 * 
	 * @return	true if the specified asset can be loaded on its own
	 */
	bool IsAssetValidForLoading( String ^AssetPathName );

	/**
	 * Wrapper for determining whether an asset is eligible to be placed in a level.
	 * 
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 * 
	 * @return	true if the specified asset can be placed in a level
	 */
	bool IsAssetValidForPlacing( String ^AssetPathName );

	/**
	 * Wrapper for determining whether an asset is eligible to be tagged.
	 * 
	 * @param	AssetFullName	the full name of the asset to check
	 * 
	 * @return	true if the specified asset can be tagged
	 */
	bool IsAssetValidForTagging( String^ AssetFullName );
};



#endif	// __ManagedCodeSupportCLR_h__

