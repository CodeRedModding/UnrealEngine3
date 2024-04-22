/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Windows;
using System.Windows.Data;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for App.xaml
	/// </summary>
	public partial class App : Application
	{
		protected override void OnStartup(StartupEventArgs e)
		{
            // Make sure the working directory is the same as the executable (can not be in that exe was in-turn launched from another, e.g the editor)
            System.IO.Directory.SetCurrentDirectory(System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location));

			Session.Init();
			Session.Current.CommandLineArgs = new FrontendCommandLine();
			Session.Current.CommandLineArgs.Parse(e.Args);

			base.OnStartup(e);
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
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			bool Val = (bool)value;
			return (Val == true ? Visibility.Collapsed : Visibility.Visible);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

	/// <summary>
	/// Converts a bool to either Visible(true) or Hidden(false)
	/// </summary>
	[ValueConversion(typeof(bool), typeof(Visibility))]
	public class BooleanToVisibilityConverter_Hidden
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			bool Val = (bool)value;
			return (Val == true ? Visibility.Visible : Visibility.Hidden);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

	/// <summary>
	/// Converts a bool to 100% opacity (true) or partial opacity (false)
	/// </summary>
	[ValueConversion(typeof(bool), typeof(Visibility))]
	public class BooleanToOpacityConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			bool Val = (bool)value;
			return (Val == true ? 1.0 : 0.25);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

	/// <summary>
	/// Converts a boolean to its opposite value
	/// </summary>
	[ValueConversion(typeof(Boolean), typeof(Boolean))]
	public class NegatingConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			Boolean BoolValue = (Boolean)value;
			return !BoolValue;
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			Boolean BoolValue = (Boolean)value;
			return !BoolValue;
		}
	}

	/// <summary>
	/// Converts any non-null object to true; null to false.
	/// </summary>
	[ValueConversion(typeof(object), typeof(bool))]
	public class IsNotNullToBoolConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return (value != null);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

	/// <summary>
	/// Converts the package mode enum to a friendly string representation.
	/// </summary>
	[ValueConversion(typeof(Pipeline.EPackageMode), typeof(String))]
	public class MobilePackageModeToString
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			try
			{
				Pipeline.EPackageMode PackageMode = (Pipeline.EPackageMode)value;
				return Pipeline.PackageIOS.EPackageMode_ToFriendlyName(PackageMode);
			}
			catch(Exception)
			{
				return "Unrecognized Package Mode";
			}
			
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

    /// <summary>
    /// Converts the package mode enum to a friendly string representation.
    /// </summary>
    [ValueConversion(typeof(Pipeline.EAndroidPackageMode), typeof(String))]
    public class AndroidPackageModeToString
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            try
            {
                Pipeline.EAndroidPackageMode PackageMode = (Pipeline.EAndroidPackageMode)value;
                return Pipeline.SyncAndroid.EAndroidPackageMode_ToFriendlyName(PackageMode);
            }
            catch (Exception)
            {
                return "Unrecognized Package Mode";
            }

        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    /// <summary>
    /// Converts the architecture enum to a friendly string representation.
    /// </summary>
    [ValueConversion(typeof(Pipeline.EAndroidArchitecture), typeof(String))]
    public class AndroidArchitectureToString
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            try
            {
                Pipeline.EAndroidArchitecture Architecture = (Pipeline.EAndroidArchitecture)value;
                return Pipeline.SyncAndroid.EAndroidArchitecture_ToFriendlyName(Architecture);
            }
            catch (Exception)
            {
                return "Unrecognized Architecture";
            }

        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

    /// <summary>
    /// Converts the texture filter enum to a friendly string representation.
    /// </summary>
    [ValueConversion(typeof(Pipeline.EAndroidTextureFilter), typeof(String))]
    public class AndroidTextureFilterToString
        : IValueConverter
    {
        /// Converts from the source type to the target type
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            try
            {
                Pipeline.EAndroidTextureFilter TextureFilter = (Pipeline.EAndroidTextureFilter)value;
                return Pipeline.SyncAndroid.EAndroidTextureFilter_ToFriendlyName(TextureFilter);
            }
            catch (Exception)
            {
                return "Unrecognized Texture Filter";
            }

        }

        /// Converts back to the source type from the target type
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            return null;	// Not supported
        }
    }

	/// <summary>
	/// Converts the package mode enum to a friendly string representation.
	/// </summary>
	[ValueConversion(typeof(Pipeline.EMacPackageMode), typeof(String))]
	public class MacPackageModeToString
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			try
			{
				Pipeline.EMacPackageMode PackageMode = (Pipeline.EMacPackageMode)value;
				return Pipeline.PackageMac.EMacPackageMode_ToFriendlyName(PackageMode);
			}
			catch (Exception)
			{
				return "Unrecognized Package Mode";
			}

		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return null;	// Not supported
		}
	}
}
