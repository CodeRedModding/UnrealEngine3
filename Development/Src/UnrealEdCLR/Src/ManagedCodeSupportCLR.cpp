/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"

using namespace System::IO;
using namespace System::Windows::Controls;
using namespace System::Windows::Markup;


namespace CLRTools
{

	/** Converts a CLR String directly to an FName */
	FName ToFName( String^ InCLRString )
	{
		pin_ptr< const TCHAR > PinnedString = PtrToStringChars( InCLRString );
		return FName( PinnedString );
	}

	/** Converts a CLR String to an FString */
	FString ToFString( String^ InCLRString )
	{
		pin_ptr< const TCHAR > PinnedString = PtrToStringChars( InCLRString );
		return FString( InCLRString->Length, PinnedString );
	}



	/** Converts an FString to a CLR String */
	String^ ToString( const FString& InFString )
	{
		return gcnew String( *InFString, 0, InFString.Len() );
	}

	/** Converts an FName to a CLRString */
	String^ FNameToString( const FName& InFName )
	{
		FString NativeString;
		InFName.ToString( NativeString );	// Out
		return gcnew String( *NativeString, 0, NativeString.Len() );
	}

	/** Converts an FString array to a CLR String array */
	List< String^ >^ ToStringArray( const TArray< FString >& InFStrings )
	{
		List< String^ >^ Strings = gcnew List< String^ >();
		Strings->Capacity = InFStrings.Num();
		for( int CurStringIndex = 0; CurStringIndex < InFStrings.Num(); ++CurStringIndex )
		{
			Strings->Add( ToString( InFStrings( CurStringIndex ) ) );
		}
		return Strings;
	}



	/** Converts a CLR String array to an FString array */
	void ToFStringArray( Generic::ICollection< String^ >^ InStrings, TArray< FString >& OutFStrings )
	{
		OutFStrings.Reset();
		OutFStrings.Reserve( InStrings->Count );
		for each( String^ CurString in InStrings )
		{
			OutFStrings.AddItem( ToFString( CurString ) );
		}
	}

	/** Converts an FColor to a CLR Color */
	Color ToColor(const FColor& InFColor )
	{
		return Color::FromRgb(InFColor.R, InFColor.G, InFColor.B);
	}


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
													bool bSizeToContent )
	{
		// Setup window source parameters
		Interop::HwndSourceParameters SourceParams( WindowTitle );	// Title
		{
			// Window position.  Can be set to CW_USEDEFAULT for default positioning by CreateWindow.
			SourceParams.PositionX = X;
			SourceParams.PositionY = Y;

			// Width and height.  Should NEVER be zero, even for auto-sized content. (MSDN says this
			// will introduce a rendering performance issue)
			SourceParams.Width = Width;
			SourceParams.Height = Height;


			// Whether we want the width and height to mean the client area size instead of the total window size
			SourceParams.AdjustSizingForNonClientArea = false;

			// Parent window handle
			SourceParams.ParentWindow = IntPtr( ParentWindow );
			
			// Window style (see http://msdn.microsoft.com/en-us/library/czada357(VS.80).aspx)
			SourceParams.WindowStyle = WindowStyle;

			// Extended window style (see http://msdn.microsoft.com/en-us/library/61fe4bte(VS.80).aspx)
			SourceParams.ExtendedWindowStyle = ExtendedWindowStyle;

			// Class style (see http://msdn.microsoft.com/en-us/library/ms633574(VS.85).aspx#class_styles)
			SourceParams.WindowClassStyle = WindowClassStyle;

			// This enables per-pixel alpha blending with the desktop behind the window and imposes various
			// limitations on window content.
			SourceParams.UsesPerPixelOpacity = bUsesPerPixelOpacity;

			// Message hook for this window.  You can use AddHook to set this on the window later.
			SourceParams.HwndSourceHook = nullptr;
		}


		// Create the window source.  This is basically a special type of window that's designed to host WPF content.
		// It wraps the regular window handle plus additional functionality.

		// NOTE: The lifetime of this will be up to the GC, but we should explicitly call Dispose on it to
		//    destroy the associated window if we want the resources released immediately!  An easy way to
		//	  do this is to wrap the returned handle in an auto_handle<>
		Interop::HwndSource^ Source = gcnew Interop::HwndSource( SourceParams );	// Parameters
		{
			// Assign WPF window to the window source
			Source->RootVisual = InWPFVisual;

			// Do we want the window to automatically size to it's content?
			Source->SizeToContent = bSizeToContent ? SizeToContent::WidthAndHeight : SizeToContent::Manual;

			// NOTE: This object has various input and visual callbacks that we may want to hook up to
		}

		// Need to make sure any faulty WPF methods are hooked as soon as possible after a WPF window
		// has been created (it loads DLLs internally, which can't be hooked until after creation)
#if WITH_EASYHOOK
		WxUnrealEdApp::InstallHooksWPF();
#endif


		// Return the handle of the newly created window
		return Source;
	}

	/** 
	 * Create a Visual from xaml 
	 */
	Visual^ CreateVisualFromXaml (String^ XamlFileName)
	{
		Visual^ NewWPFVisual = nullptr;

#if _DEBUG
		UBOOL bTryAgain = TRUE;
		while( bTryAgain )
		{
			bTryAgain = FALSE;
			try
#endif
			{
				// Allocate Xaml file reader
				auto_handle< StreamReader > MyStreamReader = gcnew StreamReader( XamlFileName );

				// Setup parser context.  This is needed so that files that are referenced from within the .xaml file
				// (such as other .xaml files or images) can be located with paths relative to the application folder.
				ParserContext^ MyParserContext = gcnew ParserContext();
				{
					// Grab the executable path
					TCHAR ApplicationPathChars[ MAX_PATH ];
					GetModuleFileName( NULL, ApplicationPathChars, MAX_PATH - 1 );
					FString ApplicationPath( ApplicationPathChars );

					// Create and assign the base URI for the parser context
					Uri^ BaseUri = Packaging::PackUriHelper::Create( gcnew Uri( CLRTools::ToString( ApplicationPath ) ) );
					MyParserContext->BaseUri = BaseUri;
				}

				// Load the file
				DependencyObject^ RootObject = 
					static_cast< DependencyObject^ >( XamlReader::Load( MyStreamReader->BaseStream, MyParserContext ) );

				// Now use the Xaml object data!
				NewWPFVisual = static_cast< Visual^ >( RootObject );
			}
#if _DEBUG
			catch( Exception^ Ex )
			{
				if( GWarn->YesNof( TEXT("Parsing %s failed: %s\r\n\r\nTry again?"), *CLRTools::ToFString(XamlFileName), *CLRTools::ToFString(Ex->Message) ) )
				{
					bTryAgain = TRUE;
				}
				else
				{
					appRequestExit(TRUE);
				}
			}
		}
#endif
		return NewWPFVisual;
	}


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
												  bool bSizeToContent )
	{
		Visual^ NewWPFVisual = CreateVisualFromXaml(XamlFileName);

		return CreateWPFWindowFromVisual(
			ParentWindow,
			NewWPFVisual,
			WindowTitle,
			X,
			Y,
			Width,
			Height,
			WindowStyle,
			ExtendedWindowStyle,
			WindowClassStyle,
			bUsesPerPixelOpacity,
			bSizeToContent );
	}



	/** Localizes the specified string and returns the result */
	String^ LocalizeString( String^ UnlocalizedString, String^ StrParam0, String^ StrParam1, String^ StrParam2 )
	{
		const UBOOL bOptional = FALSE;
		FString Localized;
			
		pin_ptr< const TCHAR > PinnedString = PtrToStringChars( UnlocalizedString );
		if( StrParam0 == nullptr )
		{
			// No params
			Localized = LocalizeUnrealEd( PinnedString );
		}
		else
		{
			pin_ptr< const TCHAR > PinnedParam0 = PtrToStringChars( StrParam0 );
			if( StrParam1 == nullptr )
			{
				// 1 param
				Localized = FString::Printf( LocalizeSecure( LocalizeUnrealEd( PinnedString ), ( TCHAR* )PinnedParam0 ) );
			}
			else
			{
				pin_ptr< const TCHAR > PinnedParam1 = PtrToStringChars( StrParam1 );
				if( StrParam2 == nullptr )
				{
					// 2 params
					Localized = FString::Printf( LocalizeSecure( LocalizeUnrealEd( PinnedString ), ( TCHAR* )PinnedParam0, ( TCHAR* )PinnedParam1 ) );
				}
				else
				{
					// 3 params
					pin_ptr< const TCHAR > PinnedParam2 = PtrToStringChars( StrParam2 );
					Localized = FString::Printf( LocalizeSecure( LocalizeUnrealEd( PinnedString ), ( TCHAR* )PinnedParam0, ( TCHAR* )PinnedParam1, ( TCHAR* )PinnedParam2 ) );
				}
			}
		}

		return gcnew String( *Localized );
	}



	/** Recursively localizes all string-based content in the specified control and it's children */
	void LocalizeContentRecursively( Visual^ VisualObj )
	{
		if( VisualObj != nullptr )
		{
			// Localize myself
			ContentControl^ Control = dynamic_cast< ContentControl^ >( VisualObj );
			if( Control != nullptr )
			{
				// We only care about strings
				String^ StringContent = dynamic_cast< String^ >( Control->Content );
				if( StringContent != nullptr )
				{
					// Update the control with the localized string data
					Control->Content = LocalizeString( StringContent );
				}


				// Also check for "headered" content controls
				HeaderedContentControl^ HeaderedControl = dynamic_cast< HeaderedContentControl^ >( Control );
				if( HeaderedControl != nullptr )
				{
					// We only care about strings
					String^ StringContent = dynamic_cast< String^ >( HeaderedControl->Header );
					if( StringContent != nullptr )
					{
						// Update the control with the localized string data
						HeaderedControl->Header = LocalizeString( StringContent );
					}
				}

			}



			// @todo WPF: This won't catch inline String objects (not sure how to modify direct String child objects
			//   in place, and reparenting seems like it would be too weird.)


			// Process logical children
			for each( Object% CurChildObj in LogicalTreeHelper::GetChildren( VisualObj ) )
			{
				Visual^ CurChildVisualObj = dynamic_cast< Visual^ >( %CurChildObj );
				if( CurChildVisualObj != nullptr )
				{
					// Recurse!
					LocalizeContentRecursively( CurChildVisualObj );
				}
			}
		}
	}



	/** Writes the specified text to the warning log */
	void LogWarningMessage( String^ Message )
	{
		pin_ptr< const TCHAR > PinnedString = PtrToStringChars( Message );

		warnf( ( TCHAR* )PinnedString );
	}

	/** 
	 * Writes the specified text to the warning log 
	 *
	 * @param	CategoryName	The name of the category for the log message.
	 * @param	Message			The message to log.
	 */
	void LogWarningMessage( EName CategoryName, String^ Message )
	{
		pin_ptr< const TCHAR > PinnedString = PtrToStringChars( Message );

		GWarn->Log( CategoryName, ( TCHAR* )PinnedString );
	}

	/**
	 * Wrapper for determining whether a package should be added to the packages tree-view.
	 *
	 * @param	Pkg		the package to check
	 *
	 * @return	FALSE if the package should be ignored.
	 */
	UBOOL IsPackageValidForTree( UPackage* Pkg )
	{
		UBOOL bResult = TRUE;

		//@todo cb  [reviewed; discuss]- should we check this as well?
		//if(EditorOnlyContentPackages.ContainsItem(Cast<UPackage>(CurGroup->GetOuter())))
		//{
		//	GroupList.Remove(x);
		//	x--;
		//}
		//else
		if ( Pkg->HasAnyFlags(RF_Transient) )
		{
			bResult = FALSE;
		}
		else if ( Pkg == UObject::GetTransientPackage() )
		{
			bResult = FALSE;
		}
		else if ( (Pkg->PackageFlags&(PKG_ContainsScript|PKG_Trash|PKG_PlayInEditor)) != 0 )
		{
			bResult = FALSE;
		}

		return bResult;
	}

	/**
	 * Utility method for extracting the package name of an asset
	 *
	 * @param	AssetPathName
	 */
	FString ExtractPackageName( const FString& AssetPathName )
	{
		INT PackageDelimiterPos = AssetPathName.InStr(TEXT("."), TRUE);
		if ( PackageDelimiterPos != INDEX_NONE )
		{
			return AssetPathName.Left(PackageDelimiterPos);
		}

		return AssetPathName;
	}

	/**
	 * Wrapper for determining whether an asset is a map package or contained in a map package
	 *
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 *
	 * @return	TRUE if the specified asset is a map package or contained in a map package
	 */
	bool IsMapPackageAsset( FString AssetPathName )
	{
		bool bResult = FALSE;
	
		AssetPathName = ExtractPackageName(AssetPathName);
		if ( AssetPathName.Len() > 0 && GPackageFileCache != NULL )
		{
			FString PackagePath;
			if ( GPackageFileCache->FindPackageFile(*AssetPathName, NULL, PackagePath) )
			{
				FFilename PackageFilename = FFilename(PackagePath);
				if ( PackageFilename.GetExtension() == FURL::DefaultMapExt )
				{
					bResult = TRUE;
				}
			}
		}

		return bResult;
	}
	bool IsMapPackageAsset( String ^AssetPathName )
	{
		return IsMapPackageAsset(ToFString(AssetPathName));
	}

	/**
	 * Wrapper for determining whether an asset is eligible to be loaded on its own.
	 * 
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 * 
	 * @return	true if the specified asset can be loaded on its own
	 */
	bool IsAssetValidForLoading( String ^AssetPathName )
	{
		return AssetPathName != nullptr && !CLRTools::IsMapPackageAsset(AssetPathName);
	}

	/**
	 * Wrapper for determining whether an asset is eligible to be placed in a level.
	 * 
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 * 
	 * @return	true if the specified asset can be placed in a level
	 */
	bool IsAssetValidForPlacing( String ^AssetPathName )
	{
		bool bResult = !IsMapPackageAsset(AssetPathName);
		if ( !bResult && AssetPathName != nullptr )
		{
			// if this map is loaded, allow the asset to be placed
			FString AssetPackageName = ExtractPackageName(ToFString(AssetPathName));
			if ( AssetPackageName.Len() > 0 )
			{
				UPackage* AssetPackage = FindObject<UPackage>(NULL, *AssetPackageName, TRUE);
				if ( AssetPackage != NULL )
				{
					// so it's loaded - make sure it is the current map
					TArray<UWorld*> CurrentMapWorlds;
					FLevelUtils::GetWorlds(CurrentMapWorlds, TRUE);
					for ( INT WorldIndex = 0; WorldIndex < CurrentMapWorlds.Num(); WorldIndex++ )
					{
						UWorld* World = CurrentMapWorlds(WorldIndex);
						if ( World != NULL && World->GetOutermost() == AssetPackage )
						{
							bResult = true;
							break;
						}
					}
				}
			}
		}
		return bResult;
	}

	/**
	 * Wrapper for determining whether an asset is eligible to be tagged.
	 * 
	 * @param	AssetFullName	the full name of the asset to check
	 * 
	 * @return	true if the specified asset can be tagged
	 */
	bool IsAssetValidForTagging( String^ AssetFullName )
	{
		return AssetFullName != nullptr && AssetFullName->Length > 0;
	}

}



#pragma unmanaged
// Implemented in Launch.cpp
extern INT GuardedMainWrapper( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, INT nCmdShow );
#pragma managed


/** Managed wrapper around GuardedMain that catches CLR exceptions and appends a managed stack trace
    before forwarding the exception to our regular structured exception handler */
INT ManagedGuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, INT nCmdShow )
{
	INT ErrorResult = 0;

	try
	{
		ErrorResult = GuardedMainWrapper( CmdLine, hInInstance, hPrevInstance, nCmdShow );
	}
	catch( Exception^ AnyException )
	{
		// If the managed call stack is basically empty then don't bother displaying it
		if( !AnyException->StackTrace->Substring( 6, 11 )->Equals( "GuardedMain" ) )
		{
			// Display the exception string
			FString ErrorString;

			ErrorString += CLRTools::ToFString( String::Format(
				"{0} error in {1}:\r\n\r\n{2}\r\n\r\n{3}",
				AnyException->GetType()->FullName,
				AnyException->Source,
				AnyException->Message,
				AnyException->StackTrace ) );

			// Also display any inner exceptions
			Exception^ CurInnerException = AnyException->InnerException;
			while( CurInnerException != nullptr )
			{
				ErrorString += CLRTools::ToFString( String::Format(
					"\nInner exception {0} in {1}:\r\n\r\n{2}\r\n\r\n{3}",
						CurInnerException->GetType()->FullName,
						CurInnerException->Source,
						CurInnerException->Message,
						CurInnerException->StackTrace ) );

				CurInnerException = CurInnerException->InnerException;
			}

			// Append to error log
			appStrcat( GErrorHist, *ErrorString );
		}

		// Pass the exception to the regular C++ structured exception handler
		throw;
	}

	return ErrorResult;
}




#pragma unmanaged


// NOTE: The following is required to avoid linker errors with WxWidgets

// @todo WPF: Why do we need to do this?  This exists in the Wx DLL somewhere and we shouldn't need
//   it linked with our app.  This problem only exists when using /clr.

void wxwxListStringNode::DeleteData()
{
    delete (wxwxListStringNode *)GetData();
}


#pragma managed


