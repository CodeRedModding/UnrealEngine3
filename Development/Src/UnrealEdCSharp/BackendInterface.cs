//=============================================================================
//	BackendInterface.cs: Allows UnrealEdCSharp to call into editor/engine code
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Text;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;
using System.Windows.Markup;
using System.IO;
using System.IO.Packaging;


namespace UnrealEd
{
	/**
	 * An Interface to be implemented by the C++/CLI portion of the code.
	 * This exists so that C# code can call C++/CLI code. 
	 */
	public interface IEditorBackendInterface
	{
		/** Writes the specified text to the warning log */
		void LogWarningMessage( String Text );
	}


	/// <summary>
	/// Singleton for the editor backend interface to UnrealEd
	/// </summary>
	public static class Backend
	{
		/// True if the backend instance has been initialized
		public static bool IsInitialized
		{
			get
			{
				return ( m_Backend != null );
			}
		}


		/// Returns the Backend instance
		public static IEditorBackendInterface Instance
		{
			get
			{
				if( m_Backend == null )
				{
					throw new InvalidOperationException( "UnrealEd.Backend.Instance requested but not initialized yet." );
				}
				return m_Backend;
			}
		}

		/// Interface exposing core engine/editor functions to C# code
		private static IEditorBackendInterface m_Backend;


		/// <summary>
		/// Initializes the backend singleton
		/// </summary>
		/// <param name="InBackendInterface">New backend interface</param>
		public static void InitBackend( IEditorBackendInterface InBackendInterface )
		{
			if( m_Backend != null )
			{
				throw new InvalidOperationException( "InitBackend should never be initialized more than once!" );
			}
			m_Backend = InBackendInterface;

			InitUnrealEdStyles(); 
		}

		/// Add the content of UnrealEdStyles into the Application's MergedDictionaries
		public static void InitUnrealEdStyles()
		{
			// Because we are hosting WPF inside ContentBrowser the WPF Application may not yet exist.
			// Make one, and merge the global resources into it.
			if ( Application.Current == null )
			{
				Application MyApp = new Application();
			}

			System.Uri ResourceLocater = new System.Uri( "/UnrealEdCSharp;component/UnrealEdStyles.xaml", System.UriKind.Relative );
			ResourceDictionary StylesResources = (ResourceDictionary)System.Windows.Application.LoadComponent( ResourceLocater );

			Application.Current.Resources.MergedDictionaries.Add( StylesResources );
		}
	}



}
