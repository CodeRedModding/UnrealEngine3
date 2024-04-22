//=============================================================================
//	Utils.cs: Unreal editor utilities in C#
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Markup;
using System.IO;
using System.IO.Packaging;
using System.Globalization;


namespace CustomControls
{
    enum LogicOp { And, Or, Nor, Nand };
    /// <summary>
    /// Compares an integer to a reference value and returns either Visible(false) or Collapsed(true)
    /// </summary>
    [ValueConversion(typeof(int), typeof(Visibility))]
    public class IntEqualsParamToVisibilityConverter
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            int Val = (int)value;
            int Param = System.Convert.ToInt32((string)parameter);
            return (Val == Param ? Visibility.Visible : Visibility.Collapsed);
        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    /// <summary>
    /// Compares an integer to a reference value and returns either Visible(false) or Collapsed(true)
    /// </summary>
    [ValueConversion(typeof(int), typeof(Visibility))]
    public class IntNotEqualsParamToVisibilityConverter
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            int Val = (int)value;
            int Param = System.Convert.ToInt32((string)parameter);
            return (Val != Param ? Visibility.Visible : Visibility.Collapsed);
        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    [ValueConversion(typeof(object), typeof(Visibility))]
    public class NotNullToVisibilityConverter
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return (value != null) ? Visibility.Visible : Visibility.Collapsed;
        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

	/// <summary>
	/// Converts a negated bool to either Visible(false) or Collapsed(true)
	/// </summary>
	[ValueConversion(typeof(bool), typeof(Visibility))]
	public class NegatedBooleanToVisibilityConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			bool Val = (bool)value;
			return (Val == true ? Visibility.Collapsed : Visibility.Visible);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

    public class MultiVisibilityConverter : IMultiValueConverter
    {
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            if (values.Length <= 0)
            {
                return Visibility.Collapsed;
            }

            LogicOp Mode = LogicOp.Or;
            if (!object.Equals(parameter, null))
            {
                Mode = (LogicOp)Enum.Parse(typeof(LogicOp), (string)parameter);
            }
            switch(Mode)
            {
                case LogicOp.And:
                    foreach (Visibility vis in values)
                    {
                        if (vis != Visibility.Visible)
                        {
                            return Visibility.Collapsed;
                        }
                    }
                    return Visibility.Visible;
                case LogicOp.Nand:
                    foreach (Visibility vis in values)
                    {
                        if (vis == Visibility.Visible)
                        {
                            return Visibility.Collapsed;
                        }
                    }
                    return Visibility.Visible;
                case LogicOp.Nor:
                    foreach (Visibility vis in values)
                    {
                        if (vis == Visibility.Collapsed)
                        {
                            return Visibility.Visible;
                        }
                    }
                    return Visibility.Collapsed;
                default:
                case LogicOp.Or:
                    foreach (Visibility vis in values)
                    {
                        if (vis != Visibility.Collapsed)
                        {
                            return Visibility.Visible;
                        }
                    }
                    return Visibility.Collapsed;
            }
        }

          /// Converts back to the source type from the target type
        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            object[] values = null;
            return values;	// Not supported
        }
    }

    public class MultiBooleanConverter
        : IMultiValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            if (values.Length <= 0)
            {
                return false;
            }
            LogicOp Mode = LogicOp.Or;
            if (!object.Equals(parameter, null))
            {
                Mode = (LogicOp)Enum.Parse(typeof(LogicOp), (string)parameter);
            }

            switch(Mode)
            {
                case LogicOp.And:
                    foreach (bool value in values)
                    {
                        if (value == false) return false;
                    }
                    return true;
                case LogicOp.Nand:
                    foreach (bool value in values)
                    {
                        if (value == true) return false;
                    }
                    return true;
                case LogicOp.Nor:
                    foreach (bool value in values)
                    {
                        if (value == false) return true;
                    }
                    return false;
                default:
                case LogicOp.Or:
                    foreach (bool value in values)
                    {
                        if (value == true) return true;
                    }
                    return false;
            }
        }

        /// Converts back to the source type from the target type
        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    public class MultiBoolToVisibilityConverter
        : IMultiValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            if (values.Length <= 0)
            {
                return Visibility.Collapsed;
            }
            LogicOp Mode = LogicOp.Or;
            if (!object.Equals(parameter, null))
            {
                Mode = (LogicOp)Enum.Parse(typeof(LogicOp), (string)parameter);
            }

            switch (Mode)
            {
                case LogicOp.And:
                    foreach (bool value in values)
                    {
                        if (value == false) return Visibility.Collapsed;
                    }
                    return Visibility.Visible;
                case LogicOp.Nand:
                    foreach (bool value in values)
                    {
                        if (value == true) return Visibility.Collapsed;
                    }
                    return Visibility.Visible;
                case LogicOp.Nor:
                    foreach (bool value in values)
                    {
                        if (value == false) return Visibility.Visible;
                    }
                    return Visibility.Collapsed;
                default:
                case LogicOp.Or:
                    foreach (bool value in values)
                    {
                        if (value == true) return Visibility.Visible;
                    }
                    return Visibility.Collapsed;
            }
        }

        /// Converts back to the source type from the target type
        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    [ValueConversion(typeof(bool), typeof(bool))]
    public class NegatedBooleanConverter
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            bool Val = (bool)value;
            return (Val == false ? true : false);
        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    [ValueConversion(typeof(int), typeof(bool))]
    public class IntGreaterOrEqualParamConverter
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            int Val = (int)value;
            int Param = System.Convert.ToInt32((string)parameter);
            return (Val >= Param ? true : false);
        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    [ValueConversion(typeof(int), typeof(bool))]
    public class IntLessOrEqualParamConverter
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            int Val = (int)value;
            int Param = System.Convert.ToInt32((string)parameter);
            return (Val >= Param ? true : false);
        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    /// <summary>
    /// Converts a AND operator for all bool values to either Visible(false) or Collapsed(true)
    /// </summary>
    [ValueConversion(typeof(bool), typeof(Visibility))]
    public class AndBooleanToVisibilityConverter
        : IMultiValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            bool Val = true;
            if (values.Length <= 0)
            {
                Val = false;
            }
            else
            {
                for (int i = 0; i < values.Length; i++)
                {
                    Val = Val && (bool)values[i];
                }
            }
            return (Val == false ? Visibility.Collapsed : Visibility.Visible);
        }

        /// Converts back to the source type from the target type
        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    /// <summary>
    /// Converts a OR operator for all bool values to either Visible(false) or Collapsed(true)
    /// </summary>
    [ValueConversion(typeof(bool), typeof(Visibility))]
    public class OrBooleanToVisibilityConverter
        : IMultiValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            bool Val = false;
            for (int i = 0; i < values.Length; i++)
            {
                Val = Val || (bool)values[i];
            }
            return (Val == false ? Visibility.Collapsed : Visibility.Visible);
        }

        /// Converts back to the source type from the target type
        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

	/// <summary>
	/// Converts a unit test status to its localized string representation
	/// (This could use a new home outside of Utils.cs, doesn't necessarily belong here)
	/// </summary>
	[ValueConversion(typeof(Enum), typeof(String))]
	public class UnitTestStatusToStringConverter : IValueConverter
	{
		public object Convert(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			String ReturnString = String.Empty;
			String DictionaryKey = "UnitTestWindow_Test" + value.ToString();
			if ( UnrealEd.Utils.ApplicationInstance.Resources.Contains( DictionaryKey ) )
			{
				ReturnString = (String)UnrealEd.Utils.ApplicationInstance.Resources[DictionaryKey];
			}
			return ReturnString;
		}

		/// <summary>
		/// Not supported
		/// </summary>
		public object ConvertBack(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			return null;
		}
	}

	/// <summary>
	/// Converts a unit test status to its corresponding brush color
	/// (This could use a new home outside of Utils.cs, doesn't necessarily belong here)
	/// </summary>
	[ValueConversion(typeof(Enum), typeof(Brush))]
	public class UnitTestStatusToBGBrushConverter : IValueConverter
	{
		public object Convert(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			SolidColorBrush BGBrush = (SolidColorBrush)parameter;
			
			// Use a special brush for unit test success or fail
			String Status = value.ToString().ToLower();
			switch (Status)
			{
				case "success":
					BGBrush = Brushes.LimeGreen;
					break;
				case "fail":
					BGBrush = Brushes.Red;
					break;
			}
			return BGBrush;
		}

		/// <summary>
		/// Not supported
		/// </summary>
		public object ConvertBack(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			return null;
		}
	}

	/// <summary>
	/// Converts a multiline string to a single line string by chopping off everything past the first line break
	/// </summary>
	[ValueConversion(typeof(String), typeof(String))]
	public class MultilineToFirstSingleLineConverter : IValueConverter
	{
		/// <summary>
		/// Converts from multiline to singleline string
		/// </summary>
		public Object Convert(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			String SingleLineString = (String)value;
			if (!String.IsNullOrEmpty(SingleLineString))
			{
				char[] Delimiters = new char[]{'\r', '\n' };
				SingleLineString = SingleLineString.Split( Delimiters )[0];
			}
			return SingleLineString;
		}

		/// <summary>
		/// Not supported
		/// </summary>
		public Object ConvertBack(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			return null;
		}
	}

	/// <summary>
	/// Converts an int representing bytes to a double representing the same value expressed in megabytes (MB)
	/// (This could use a new home outside of Utils.cs, doesn't necessarily belong here)
	/// </summary>
	[ValueConversion(typeof(int), typeof(double))]
	public class ByteToMegabyteConverter : IValueConverter
	{
		/// <summary>
		/// Convert from bytes to megabytes
		/// </summary>
		public Object Convert(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			int NumBytes = (int)value;
			return NumBytes / (1024.0 * 1024.0); 
		}

		/// <summary>
		/// Not supported
		/// </summary>
		public Object ConvertBack(Object value, Type targetType, Object parameter, CultureInfo culture)
		{
			return null;
		}
	}

	/// <summary>
	/// Convert from a string representing a Source Control action to its corresponding image
	/// (This could use a new home outside of Utils.cs, doesn't necessarily belong here)
	/// </summary>
	[ValueConversion(typeof(String), typeof(System.Windows.Media.Imaging.BitmapImage))]
	public class SCCActionToIconConverter : IValueConverter
	{
		/// <summary>
		/// Convert from SCC action to corresponding image
		/// </summary>
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			String SCCAction = (String)value;
			String ResourceKey;
			switch (SCCAction)
			{
				case "add":
					ResourceKey = "imgSCC_Action_Add";
					break;

				case "edit":
					ResourceKey = "imgSCC_Action_Edit";
					break;

				case "delete":
					ResourceKey = "imgSCC_Action_Delete";
					break;

				case "branch":
					ResourceKey = "imgSCC_Action_Branch";
					break;

				case "integrate":
					ResourceKey = "imgSCC_Action_Integrate";
					break;

				default:
					ResourceKey = "imgSCC_Action_Edit";
					break;
			}

			return UnrealEd.Utils.ApplicationInstance.Resources[ResourceKey];
		}

		/// <summary>
		/// Not supported
		/// </summary>
		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			return null;
		}
	}
}

namespace UnrealEd
{
	/// <summary>
	/// Radio button that works around bugs in WPF's radio button that prevents it from being
	/// used with bindings.  Bind to "IsActuallyChecked" property to get the results you want!
	///
	/// NOTE: WPF currently has a bug where RadioButtons won't propagate "unchecked" state to bound properties.
	///		 We work around this by using an event handler to make sure our properties are in sync.
	/// </summary>
	public class BindableRadioButton : RadioButton
	{
		/// Constructor
		public BindableRadioButton()
		{
			this.Checked += new RoutedEventHandler( 
					delegate( Object Sender, RoutedEventArgs Args )
					{
						if( !bIsChanging )
						{
							this.IsActuallyChecked = true;
						}
					}
				);

			this.Unchecked += new RoutedEventHandler(
					delegate( Object Sender, RoutedEventArgs Args )
					{
						if( !bIsChanging )
						{
							this.IsActuallyChecked = false;
						}
					}
				);
		}


		/// Stores whether or not the radio button is actually checked or not
		public bool IsActuallyChecked
		{
			get
			{
				return (bool)GetValue( IsActuallyCheckedProperty );
			}
			set
			{
				SetValue( IsActuallyCheckedProperty, value );
			}
		}


		/// Bind to IsActuallyChecked to get the correct behavior with your radio buttons!
		public static readonly DependencyProperty IsActuallyCheckedProperty =
			DependencyProperty.Register( "IsActuallyChecked", typeof( bool ), typeof( BindableRadioButton ),
										 new FrameworkPropertyMetadata( false, FrameworkPropertyMetadataOptions.Journal | FrameworkPropertyMetadataOptions.BindsTwoWayByDefault, OnIsActuallyCheckedChanged ) );

		/// Called when IsActuallyChecked property has changed
		public static void OnIsActuallyCheckedChanged( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			bIsChanging = true;
			( (BindableRadioButton)d ).IsChecked = (bool)e.NewValue;
			bIsChanging = false;
		}


		/// Static: True while IsActuallyChecked is being updated
		static bool bIsChanging = false;
	}	
	
	public partial class NativeMethods
	{
		/// SetCursorPos (http://msdn.microsoft.com/en-us/library/ms648394(VS.85).aspx)
		[System.Runtime.InteropServices.DllImportAttribute( "user32.dll", EntryPoint = "SetCursorPos" )]
		[return: System.Runtime.InteropServices.MarshalAsAttribute( System.Runtime.InteropServices.UnmanagedType.Bool )]
		public static extern bool SetCursorPos( int X, int Y );
	}


	public static class Utils
	{

		static Utils()
		{
			// Only force-load resources when in Expression Blend.  Must be done before InitializeComponent is called.

			// Blend will always have an application and a main window set.  The editor however, may not have
			// a main window.
			if ( Application.Current != null && Application.Current.MainWindow != null )
			{
				// Are we in Expression Blend?
				if ( DesignerProperties.GetIsInDesignMode( Application.Current.MainWindow ) )
				{
					Utils.LoadFloatingResourcesForDesigner();
				}
			}
		}

		/// <summary>
		/// Case insensitive test that checks if a String is within a List of Strings.
		/// </summary>
		/// <param name="Token">The token to look for</param>
		/// <param name="StringList">List of string to compare to token</param>
		/// <returns>True if the Tag is in the TagList</returns>
		public static bool ListContainsString( String Token, ICollection<String> StringList, bool IgnoreCase )
		{
			foreach ( String TokenFromList in StringList )
			{
				if ( 0 == String.Compare( Token, TokenFromList, IgnoreCase ) )
				{
					return true;
				}
			}

			return false;
		}


        /// <summary>
        /// Checks if a partial String is within any String in a List.
        /// </summary>
        /// <param name="Token">The token to look for</param>
        /// <param name="StringList">List of string to compare to token</param>
        /// <param name="IgnoreCase">True when case should be ignored in the search</param>
        /// <returns>True if the String is within any String in the StringList</returns>
        public static bool ListContainsPartialString(String Token, ICollection<String> StringList, bool IgnoreCase)
        {
            if ( IgnoreCase )
            {
                String UpperToken = Token.ToUpper();
                foreach (String TokenFromList in StringList)
                {
                    if ( TokenFromList.ToUpper().Contains(UpperToken) )
                    {
                        return true;
                    }
                }
            }
            else
            {
                foreach (String TokenFromList in StringList)
                {
                    if (TokenFromList.Contains(Token))
                    {
                        return true;
                    }
                }
            }

            return false;
        }


		/// <summary>
		/// Loads "floating" WPF resources from disk and merges them into the application's resource dictionary
		/// </summary>
		public static void LoadFloatingResourcesForDesigner()
		{
			string ApplicationPath = Environment.CurrentDirectory;


			// Setup parser context.  This is needed so that files that are referenced from within the .xaml file
			// (such as other .xaml files or images) can be located with paths relative to the application folder.
			ParserContext MyParserContext = new ParserContext();
			{
				// Create and assign the base URI for the parser context
				Uri BaseUri = PackUriHelper.Create( new Uri( ApplicationPath ) );
				MyParserContext.BaseUri = BaseUri;
			}


			// Load localized resources (default language = INT)
			{
				string WPFResourcePath = Path.Combine( ApplicationPath, "..\\..\\..\\Engine\\EditorResources\\WPF\\Localized\\" );

				// Load all of the .xaml files we find in the WPF resource dictionary folder
				foreach( string CurResourceFileName in Directory.GetFiles( WPFResourcePath, "*.INT.xaml" ) )
				{
					string WPFResourcePathAndFileName = Path.Combine( WPFResourcePath, CurResourceFileName );

					try
					{
						// Allocate Xaml file reader
						using( StreamReader MyStreamReader = new StreamReader( WPFResourcePathAndFileName ) )
						{
							// Load the file
							ResourceDictionary MyDictionary = (ResourceDictionary)XamlReader.Load( MyStreamReader.BaseStream, MyParserContext );

							// Add to the application's list of dictionaries
							Application.Current.Resources.MergedDictionaries.Add( MyDictionary );
						}
					}

					catch( Exception E )
					{
						// Error loading .xaml file
						throw E;
					}
				}
			}

			// Add the content of UnrealEdStyles into the Application's MergedDictionaries
			{
				Backend.InitUnrealEdStyles();
			}

		}



		/// <summary>
		/// Returns the instance of the currently running application
		/// </summary>
		/// <returns>The application</returns>
		public static Application ApplicationInstance
		{
			get
			{
				return Application.Current;
			}
		}


		/// <summary>
		/// Returns a localized string
		/// </summary>
		/// <param name="LocDictionaryKey">Key of message to look up in localization resource dictionary</param>
		/// <returns>Localized string associated with the key</returns>
		public static String Localize( String LocDictionaryKey )
		{
			String LocString = (String)Utils.ApplicationInstance.TryFindResource( LocDictionaryKey );
			if( LocString == null )
			{
				LocString = String.Format( "<MissingLoc:{0}>", LocDictionaryKey );
			}
			return LocString;
		}

		/// <summary>
		/// Returns a formatted and localized string; performs argument substitution.
		/// </summary>
		/// <param name="LocDictionaryKey">Key of message to look up in localization resource dictionary</param>
		/// <param name="Args">Parameters to substitute in the localized string</param>
		/// <returns>Localized and formatted string.</returns>
		public static String Localize( String LocDictionaryKey, params object[] Args )
		{
			String Message = Localize( LocDictionaryKey );
			return String.Format( Message, Args );
		}

		/// Performance version of Localize
		public static String Localize( String LocDictionaryKey, Object Arg0 )
		{
			String Message = Localize( LocDictionaryKey );
			return String.Format( Message, Arg0 );
		}

		/// Performance version of Localize
		public static String Localize( String LocDictionaryKey, Object Arg0, Object Arg1 )
		{
			String Message = Localize( LocDictionaryKey );
			return String.Format( Message, Arg0, Arg1 );
		}

		/// Performance version of Localize
		public static String Localize( String LocDictionaryKey, Object Arg0, Object Arg1, Object Arg2 )
		{
			String Message = Localize( LocDictionaryKey );
			return String.Format( Message, Arg0, Arg1, Arg2 );
		}


		/// <summary>
		/// Utility function to create a binding that also takes a value converter
		/// </summary>
		/// <param name="TargetElement">Instance of FrameworkElement that contains the target property</param>
		/// <param name="TargetProperty">Target property (typically static)</param>
		/// <param name="SourceObject">Instance of DependencyObject that contains the source property</param>
		/// <param name="SourcePropertyName">String name of the source property to bind with</param>
		/// <param name="Converter">Optional value converter (null for none.)</param>
		/// <param name="ConverterParam">Optional parameter for value converter (null for none.)</param>
		public static void CreateBinding( FrameworkElement TargetElement, DependencyProperty TargetProperty, Object SourceObject, String SourcePropertyName, IValueConverter Converter, Object ConverterParam )
		{
			Binding MyBinding = new Binding( SourcePropertyName );
			MyBinding.Source = SourceObject;
			MyBinding.Converter = Converter;
			MyBinding.ConverterParameter = ConverterParam;
			TargetElement.SetBinding( TargetProperty, MyBinding );
		}

	
		
		/// <summary>
		/// Utility function to create a binding that also takes a value converter
		/// </summary>
		/// <param name="TargetElement">Instance of FrameworkElement that contains the target property</param>
		/// <param name="TargetProperty">Target property (typically static)</param>
		/// <param name="SourceObject">Instance of DependencyObject that contains the source property</param>
		/// <param name="SourcePropertyName">String name of the source property to bind with</param>
		/// <param name="Converter">Optional value converter (null for none.)</param>
		public static void CreateBinding( FrameworkElement TargetElement, DependencyProperty TargetProperty, Object SourceObject, String SourcePropertyName, IValueConverter Converter )
		{
			Binding MyBinding = new Binding( SourcePropertyName );
			MyBinding.Source = SourceObject;
			MyBinding.Converter = Converter;
			TargetElement.SetBinding( TargetProperty, MyBinding );
		}



		/// <summary>
		/// Utility function to create a binding
		/// </summary>
		/// <param name="TargetElement">Instance of FrameworkElement that contains the target property</param>
		/// <param name="TargetProperty">Target property (typically static)</param>
		/// <param name="SourceObject">Instance of DependencyObject that contains the source property</param>
		/// <param name="SourcePropertyName">String name of the source property to bind with</param>
		public static void CreateBinding( FrameworkElement TargetElement, DependencyProperty TargetProperty, Object SourceObject, String SourcePropertyName )
		{
			CreateBinding( TargetElement, TargetProperty, SourceObject, SourcePropertyName, null );
		}


		/// <summary>
		/// Utility function to clear a binding
		/// </summary>
		/// <param name="TargetElement">Instance of FrameworkElement that contains the target property</param>
		/// <param name="TargetProperty">Target property (typically static)</param>
		public static void ClearBinding( FrameworkElement TargetElement, DependencyProperty TargetProperty )
		{
			BindingOperations.ClearBinding( TargetElement, TargetProperty );
		}


		/// <summary>
		/// Debug method. Builds the subtree of the InNode into a string.
		/// </summary>
		/// <param name="InNode">FrameworkElement to serve as the root of our tree.</param>
		/// <returns>String representation of this node and all of its children.</returns>
		public static String VisualTreeToString( FrameworkElement InNode )
		{
			StringBuilder TreeStringBuilder = new StringBuilder();
			BuildVisualTreeString( TreeStringBuilder, InNode, 0 );
			return TreeStringBuilder.ToString();
		}

		/// <summary>
		/// Helper to VisualTreeToString. Prints InNode and all of its children into the OutVisualTreeString.
		/// </summary>
		/// <param name="OutVisualTreeString">StringBuilder that receives the output.</param>
		/// <param name="InNode">The Node to output to string</param>
		/// <param name="IndentLevel">The indentation level whith which to show this node.</param>
		private static void BuildVisualTreeString( StringBuilder OutVisualTreeString, FrameworkElement InNode, int IndentLevel )
		{

			int NumChildren = VisualTreeHelper.GetChildrenCount(InNode);

			if ( InNode != null )
			{
				for ( int i = 0; i < IndentLevel; i++ )
				{
					OutVisualTreeString.Append( "|    " );
				}

				char BulletChar = ( NumChildren > 0 ) ? '+' : '|';
				OutVisualTreeString.Append( BulletChar );
				OutVisualTreeString.Append( " " );
				
				OutVisualTreeString.Append( InNode.GetType().ToString() );
				OutVisualTreeString.Append( "( " );
				OutVisualTreeString.Append( InNode.Name );
				OutVisualTreeString.Append( " )\n" );
			}

			

			for ( int ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx )
			{
				FrameworkElement ChildNode = VisualTreeHelper.GetChild( InNode, ChildIdx ) as FrameworkElement;
				if ( ChildNode != null )
				{
					BuildVisualTreeString( OutVisualTreeString, ChildNode, IndentLevel + 1 );
				}
			}
		}

		/// <summary>
		/// Converts a string representing the path to a package on disk into a standard format where the first part of the
		/// converted name is the name of the immediate subdirectory of the root UnrealEngine3 folder. i.e. 
		/// ..\..\Engine\Content\ BECOMES Engine\Content\
		/// 
		/// D:\UnrealEngine3\Engine\Content BECOMES Engine\Content
		/// </summary>
		/// <param name="PackagePathName">file name (including relative or absolute path) for package</param>
		/// <param name="RootDirectory">path to root directory of the game (i.e. C:\Code\UnrealEngine3\)</param>
		/// <param name="StartupDirectory">path to directory containing the image being executed (i.e. C:\Code\UnrealEngine3\Binaries\Win32)</param>
		/// <returns>see comments above</returns>
		public static String NormalizePackageFilePathName( String PackagePathName, String RootDirectory, String StartupDirectory )
		{
			String InValue = PackagePathName;
			if ( PackagePathName.StartsWith(StartupDirectory + "..", StringComparison.OrdinalIgnoreCase) )
			{
				PackagePathName = PackagePathName.Substring(StartupDirectory.Length);
			}
			else if( PackagePathName.StartsWith( RootDirectory, StringComparison.OrdinalIgnoreCase ) )
			{
				PackagePathName = PackagePathName.Substring(RootDirectory.Length);
			}

			if ( PackagePathName.StartsWith("..") )
			{
				int pos = PackagePathName.IndexOf(System.IO.Path.DirectorySeparatorChar);
				while ( pos > 0 && PackagePathName.StartsWith("..") )
				{
					PackagePathName = PackagePathName.Substring(pos + 1);
				}
			}

			return PackagePathName;
		}

		/// <summary>
		/// Determine whether a package should be ignored by the content browser
		/// </summary>
		/// <param name="PackageFilePathName">the fully qualified path name to the package file</param>
		/// <returns>true if this package is a special package which should NOT be displayed in the content browser</returns>
		public static bool IsPackageValidForTree( String PackageFilePathName )
		{
			String PackageName = System.IO.Path.GetFileNameWithoutExtension(PackageFilePathName);
			bool bInvalidPackage = PackageName.StartsWith("LocalShaderCache-")
								|| PackageName.StartsWith("RefShaderCache-")
								|| PackageName.EndsWith("ContentTagsIndex")
								|| IsScriptPackageName(PackageFilePathName)
								|| PackageFilePathName.Contains("__Trashcan");

			return !bInvalidPackage;
		}

		/// <summary>
		/// Wrapper method for checking whether the specified filename contains the
		/// script output directory.
		/// </summary>
		/// <param name="PackageFilePathName">the file pathname to check</param>
		/// <returns>true if the file pathname contains the script output directory (considers both normal and final_release)</returns>
		public static bool IsScriptPackageName( String PackageFilePathName )
		{
			//@todo cb [reviewed; do it!] - eventually need to change this to check script output path (both normal and FR)
			String ScriptOutputFolderName = "Script" + System.IO.Path.DirectorySeparatorChar;
			if ( PackageFilePathName.Contains(ScriptOutputFolderName) )
			{
				return true;
			}

			ScriptOutputFolderName = "FinalReleaseScript" + System.IO.Path.DirectorySeparatorChar;
			if ( PackageFilePathName.Contains(ScriptOutputFolderName) )
			{
				return true;
			}

			return false;
		}

	
		
		/// <summary>
		/// Returns the class name part of an Unreal object full name string
		/// </summary>
		/// <param name="InFullName">Full name string (e.g: class package.[group.]name)</param>
		/// <returns>The class name string</returns>
		public static string GetClassNameFromFullName( string InFullName )
		{
			int AssetNameStartIndex = InFullName.IndexOf( " " );
			if( AssetNameStartIndex < 1 )	// Must have at least one character in the class name
			{
				throw new ArgumentException( "Full name does not appear to contain an object class name" );
			}

			// Return the class name part of the string
			return InFullName.Substring( 0, AssetNameStartIndex );
		}


		/// <summary>
		/// Returns the fully qualified path part of an Unreal object full name string
		/// </summary>
		/// <param name="InFullName">Full name string (e.g: class package.[group.]name)</param>
		/// <returns>The fully qualified path string (e.g: package.[group.]name)</returns>
		public static string GetPathFromFullName( string InFullName )
		{
			int AssetNameStartIndex = InFullName.IndexOf( " " );
			if( AssetNameStartIndex < 1 )	// Must have at least one character in the class name
			{
				throw new ArgumentException( "Full name does not appear to contain an object class name" );
			}

// 			// Full names must end with a single quote character
// 			if( InFullName[ InFullName.Length - 1 ] != '\'' )
// 			{
// 				throw new ArgumentException( "Bad syntax for object full name" );
// 			}
// 
			// Return the path name part of the string
			int NumPrefixChars = AssetNameStartIndex + 1;
			int NumPathChars = InFullName.Length - NumPrefixChars;
			return InFullName.Substring( NumPrefixChars, NumPathChars );
		}


		/// <summary>
		/// Creates an object full name string from a class name and a fully qualified object path
		/// </summary>
		/// <param name="InClassName">Name of the object's class</param>
		/// <param name="InFullyQualifiedPath">Object's fully qualified path</param>
		/// <returns>Object full name string (e.g: class'package.[group.]name)</returns>
		public static string MakeFullName( string InClassName, string InFullyQualifiedPath )
		{
			return InClassName + " " + InFullyQualifiedPath;
		}
	}




	public static class MathUtils
	{
		/// <summary>
		/// Clamps the value between min and max.
		/// </summary>
		/// <param name="Value">The value to clamp.</param>
		/// <param name="Min">The low boundary.</param>
		/// <param name="Max">The high boundary.</param>
		/// <returns></returns>
		public static double Clamp( double Value, double Min, double Max )
		{
			if ( Value > Max )
			{
				return Max;
			}
			if ( Value < Min )
			{
				return Min;
			}
			return Value;
		}

		/// <summary>
		/// Clamps the value between min and max.
		/// </summary>
		/// <param name="Value">The value to clamp.</param>
		/// <param name="Min">The low boundary.</param>
		/// <param name="Max">The high boundary.</param>
		/// <returns></returns>
		public static int Clamp( int Value, int Min, int Max )
		{
			if ( Value > Max )
			{
				return Max;
			}
			if ( Value < Min )
			{
				return Min;
			}
			return Value;
		}
	}




	/// <summary>
	/// ScopedPerfTimer is designed to be called from a "using" statement such that the Dispose
	/// method is called automatically after exiting the scoped section of code
	/// </summary>
	public class ScopedPerfTimer : IDisposable
	{
		/// Name of the timer
		private String m_Name = null;

		/// Stopwatch for generating timing results
		private Stopwatch m_Stopwatch;


		/// Constructor
		public ScopedPerfTimer()
		{
			m_Stopwatch = new Stopwatch();
			m_Stopwatch.Start();
		}



		/// Constructor
		public ScopedPerfTimer( String Name )
		{
			m_Name = Name;

			m_Stopwatch = new Stopwatch();
			m_Stopwatch.Start();
		}




		/// Called when the object is either disposed or finalized; will only be called once.
		void OnDestruction()
		{
			PrintElapsedTime();

			m_Stopwatch = null;
		}


		/// Deterministic destruction
		public void Dispose()
		{
			OnDestruction();

			// Tell the GC not to bother calling the finalizer
			GC.SuppressFinalize( this );
		}


		/// Non-deterministic finalization
		~ScopedPerfTimer()
		{
			if( m_Stopwatch != null )
			{
				throw new Exception( "ScopedPerfTimer was garbage collected before being disposed!  This usually means that you forgot to wrap the construction in a C# 'using' statement.  In C++/CLI, you can simply use stack semantics or operator delete when you're done with the timer." );
			}

			OnDestruction();
		}


		/// Prints the time elapsed
		public void PrintElapsedTime()
		{
			TimeSpan ElapsedTime = m_Stopwatch.Elapsed;

#if TRACE
			if( m_Name != null )
			{
				System.Diagnostics.Trace.WriteLine( String.Format( "[ScopedPerfTimer:{0}] Elapsed milliseconds = {1}", m_Name, ElapsedTime.TotalMilliseconds ) );
			}
			else
			{
				System.Diagnostics.Trace.WriteLine( String.Format( "[ScopedPerfTimer] Elapsed milliseconds = {0}", ElapsedTime.TotalMilliseconds ) );
			}
#endif
		}


	}



	/// <summary>
	/// One-dimensional spring simulation
	/// </summary>
	public class DoubleSpring
	{

		/// Target position for the spring
		public double TargetPosition { get { return m_TargetPosition; } set { m_TargetPosition = value; } }
		public double m_TargetPosition = 0.0;

		/// Current position of the spring
		public double Position
		{
			get
			{
				return m_Position;
			}
			set
			{
				m_Position = value;
				PreviousTargetPosition = value;
			}
		}
		public double m_Position = 0.0;

		/// Spring constant (how springy, lower values = more springy!)
		public double SpringConstant { get { return m_SpringConstant; } set { m_SpringConstant = value; } }
		public double m_SpringConstant = 6.0;

		/// Length of the spring
		public double SpringLength { get { return m_SpringLength; } set { m_SpringLength = value; } }
		public double m_SpringLength = 0.1;

		/// Damp constant
		public double DampConstant { get { return m_DampConstant; } set { m_DampConstant = value; } }
		public double m_DampConstant = 0.05;

		/// Epsilon for snapping position and velocity
		public double SnappingEpsilon { get { return m_SnappingEpsilon; } set { m_SnappingEpsilon = value; } }
		public double m_SnappingEpsilon = 0.01;



		/// <summary>
		/// Default constructor
		/// </summary>
		public DoubleSpring()
		{
		}



		/// <summary>
		/// Constructor that takes an initial position
		/// </summary>
		/// <param name="InInitialPosition">Initial spring position</param>
		public DoubleSpring( double InInitialPosition )
		{
			Position = InInitialPosition;
		}


		/// <summary>
		/// Update the spring simulation
		/// </summary>
		/// <param name="InQuantum">Time passed since last update</param>
		public void Update( double InQuantum )
		{
			double Disp = Position - TargetPosition;
			double DispLength = Math.Abs( Disp );
			if( DispLength > SnappingEpsilon )
			{
				double ForceDirection = (double)Math.Sign( Disp );

				double TargetDisp = PreviousTargetPosition - TargetPosition;
				double VelocityOfTarget = TargetDisp * InQuantum;
				double DistBetweenDisplacements = Math.Abs( Disp - VelocityOfTarget );

				double ForceAmount =
					SpringConstant * ( SpringLength - DispLength ) +
					DampConstant * DistBetweenDisplacements;

				Position += ForceDirection * ForceAmount * InQuantum;
			}

			// Snap new position to target if we're close enough
			if( Math.Abs( Position - TargetPosition ) < SnappingEpsilon )
			{
				Position = TargetPosition;
			}

			PreviousTargetPosition = TargetPosition;
		}


		/// Previous target position of the spring
		double PreviousTargetPosition { get { return m_PreviousTargetPosition; } set { m_PreviousTargetPosition = value; } }
		double m_PreviousTargetPosition = 0.0;
	}


	/// A class that is a case-insensitive set of string and does not require .NET3.5.
	public class NameSet : Dictionary<String, int>, IEnumerable<String>
	{
		/// Construct an empty NameSet.
		public NameSet() : base(StringComparer.OrdinalIgnoreCase)
		{
		}

		/// Construct a NameSet from a collection
		public NameSet( ICollection<String> InSource ) : base(StringComparer.OrdinalIgnoreCase)
		{
			foreach( String Str in InSource )
			{
				this.Add(Str);
			}
		}

		/// <summary>
		/// Add a name. The name is only added if it is not already in the set.
		/// </summary>
		public void Add( String NameToAdd )
		{
			if ( !Contains(NameToAdd) )
			{
				base.Add( NameToAdd, 0 );
			}			
		}

		/// <summary>
		/// Does the NameSet contain the NameToFind?
		/// </summary>
		/// <returns>True if the NameToFind is in the NameSet.</returns>
		public bool Contains( String NameToFind )
		{
			return base.ContainsKey( NameToFind );
		}



		#region IEnumerable<String> Members

		/// Support Enumerable so that foreach works.
		public new IEnumerator<String> GetEnumerator()
		{
			return ( this.Keys as IEnumerable<String> ).GetEnumerator();
		}

		#endregion

		/// A more verbose ToString().
		public override string ToString()
		{
			StringBuilder StringRepresentationBuilder = new StringBuilder();
			StringRepresentationBuilder.AppendFormat( "{0} Names:", this.Count );
			StringRepresentationBuilder.AppendLine();
			foreach ( String Name in this )
			{
				StringRepresentationBuilder.AppendLine( Name );
			}

			return StringRepresentationBuilder.ToString();
		}
	}

	public static class ColorUtil
	{

		public static void RGBToHSV( float R, float G, float B, out float H, out float S, out float V )
		{
			float RGBMax = Math.Max( Math.Max( R, G ), B );
			float RGBMin = Math.Min( Math.Min( R, G ), B );

			float RGBRange = RGBMax - RGBMin;

			H = ( RGBMax == RGBMin ? 0.0f :
					   RGBMax == R ? ( ( ( ( G - B ) / RGBRange ) * 60.0f ) + 360.0f ) % 360.0f :
					   RGBMax == G ? ( ( ( B - R ) / RGBRange ) * 60.0f ) + 120.0f :
					   RGBMax == B ? ( ( ( R - G ) / RGBRange ) * 60.0f ) + 240.0f :
					   0.0f );

			S = ( RGBMax == 0.0f ) ? 0.0f : ( ( RGBMax - RGBMin ) / RGBMax );

			V = RGBMax;
		}

		public static void HSVtoRGB( float Hue, float Saturation, float Value, out float R, out float G, out float B )
		{
			float HDiv60 = Hue / 60.0f;
			float HDiv60_Floor = (float)Math.Floor( HDiv60 );
			float HDiv60_Fraction = HDiv60 - HDiv60_Floor;

			float[] RGBValues = {
	            Value,
	            Value * (1.0f - Saturation),
	            Value * (1.0f - (HDiv60_Fraction * Saturation)),
	            Value * (1.0f - ((1.0f - HDiv60_Fraction) * Saturation)),
	        };

			uint[,] RGBSwizzle = {
	            {0, 3, 1},
	            {2, 0, 1},
	            {1, 0, 3},
	            {1, 2, 0},
	            {3, 1, 0},
	            {0, 1, 2},
	        };

			uint SwizzleIndex = ( (uint)HDiv60_Floor ) % 6;

			R = RGBValues[ RGBSwizzle[SwizzleIndex, 0] ];
			G = RGBValues[ RGBSwizzle[SwizzleIndex, 1] ];
			B = RGBValues[ RGBSwizzle[SwizzleIndex, 2] ];
		}
	}

	/// <summary>
	/// Class used by the NewMapScreen to hold data displayed via a DataTemplate in an ItemsControl
	/// </summary>
	public class TemplateMapMetadata
	{
		public string PackageName { get; set; }
		public string DisplayName { get; set; }
		public BitmapSource Thumbnail { get; set; }
	}

	/// <summary>
	/// Converter returns value * paramter as double
	/// </summary>
	public class MultiplyingConverter : IValueConverter
	{
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			if (null == value || null == parameter) return 0.0;
			double a = System.Convert.ToDouble(value);
			double b = System.Convert.ToDouble(parameter);
			return a * b;
		}

		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			throw new NotImplementedException();
		}
	}

}
