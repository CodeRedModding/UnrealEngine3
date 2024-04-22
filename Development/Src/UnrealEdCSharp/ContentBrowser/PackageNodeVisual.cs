/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using UnrealEd;
using System.Globalization;

namespace ContentBrowser
{
	public class ObjectContainerNodeVisual : CustomControls.TreeNodeVisual
	{
	}

	public class GroupNodeVisual : ObjectContainerNodeVisual
	{
	}

	public class PackageNodeVisual : ObjectContainerNodeVisual
	{
	}

	/// <summary>
	/// Applies bold to package item text if the package is writable
	/// </summary>
	[ValueConversion(typeof(ObjectContainerNode.TreeNodeIconType), typeof(FontWeight))]
	public class SCCStateToFontWeightConverter : IValueConverter
	{
		/// <summary>
		/// Convert package status to font weight. e.g. loaded packages are bold.
		/// </summary>
		/// <param name="value">Package status value to convert</param>
		/// <returns>FontWeight result of conversion.</returns>
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			ObjectContainerNode.TreeNodeIconType CurrentState = (ObjectContainerNode.TreeNodeIconType)value;
			switch ( CurrentState )
			{
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut:
				case ObjectContainerNode.TreeNodeIconType.ICON_NotInDepot:
					return FontWeights.Bold;

				default:
					return FontWeights.Normal;
			}
		}

		/// Do nothing.
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;
		}
	}

	/// <summary>
	/// Applies foreground/background color to package item text depending on selection state and loaded status
	/// </summary>
	[ValueConversion(typeof(ObjectContainerNode.PackageStatus), typeof(Brush))]
	public class PackageStateToFontColorConverter : IMultiValueConverter
	{
		#region IMultiValueConverter Members

		/// <summary>
		/// Convert PackageStatus to font color. e.g. Grey out unloaded packages.
		/// </summary>
		public object Convert( object[] values, Type targetType, object parameter, CultureInfo culture )
		{
			ObjectContainerNode.PackageStatus loadedStatus = ((ObjectContainerNode.PackageStatus)values[0]);
			bool IsSelected = (bool)values[1];

			if ( IsSelected )
			{
				return SystemColors.ControlTextBrush;
			}
			else
			{
				switch ( loadedStatus )
				{
					case ObjectContainerNode.PackageStatus.NotLoaded:
					{
						return Brushes.Gray;
					}
					case ObjectContainerNode.PackageStatus.PartiallyLoaded:
					{
						return Application.Current.Resources["Slate_ListItem_Foreground"];
					}
					case ObjectContainerNode.PackageStatus.FullyLoaded:
					{
						return Brushes.White;
					}
					default:
						return Application.Current.Resources["Slate_ListItem_Foreground"];
						
				}
			}

		}

		/// Do nothing.
		public object[] ConvertBack( object value, Type[] targetTypes, object parameter, CultureInfo culture )
		{
			throw new Exception( "The method or operation is not implemented." );
		}

		#endregion
	}

	/// <summary>
	/// Applies an icon to indicate SCC status for package items
	/// </summary>
	[ValueConversion(typeof(ObjectContainerNode.TreeNodeIconType), typeof(BitmapImage))]
	public class SCCStateToIconConverter : IValueConverter
	{
		#region IValueConverter Members
		/// <summary>
		/// Converts SCC state (for packages) or expanded state (for folders) into an icon that is displayed in the tree view
		/// </summary>
		/// <param name="value">the TreeNodeIconState for a node in the tree</param>
		/// <returns>an ImageSource object which represents that TreeNodeIconState.</returns>
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			ObjectContainerNode.TreeNodeIconType CurrentState = (ObjectContainerNode.TreeNodeIconType)value;
			String imgKey;
			switch ( CurrentState )
			{
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut:
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedIn:
				case ObjectContainerNode.TreeNodeIconType.ICON_NotCurrent:
				case ObjectContainerNode.TreeNodeIconType.ICON_NotInDepot:
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedOutOther:
					imgKey = "img_CB_Package";
					break;
				case ObjectContainerNode.TreeNodeIconType.ICON_Group:
					imgKey = "img_CB_Group";
					break;
				case ObjectContainerNode.TreeNodeIconType.ICON_Unknown:
					imgKey = "img_CB_Package";
					break;

				case ObjectContainerNode.TreeNodeIconType.ICON_Ignore:
				default:
					return null;
			}

			return Utils.ApplicationInstance.Resources[ imgKey ];
		}

		/// Do nothing.
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;
		}
		#endregion
	}

	/// <summary>
	/// Applies an OverlayIcon to indicate SCC status for package items
	/// </summary>
	[ValueConversion( typeof( ObjectContainerNode.TreeNodeIconType ), typeof( BitmapImage ) )]
	public class SCCStateToOverlayIconConverter : IValueConverter
	{
		#region IValueConverter Members
		/// <summary>
		/// Converts SCC state (for packages) into an overlay icon that is displayed in the tree view.
		/// </summary>
		/// <param name="value">the TreeNodeIconState for a node in the tree</param>
		/// <returns>an ImageSource object which represents that TreeNodeIconState.</returns>
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			ObjectContainerNode.TreeNodeIconType CurrentState = (ObjectContainerNode.TreeNodeIconType)value;
			String imgKey;
			switch ( CurrentState )
			{
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut:
				    imgKey = "imgCheckedOut";
				    break;
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedIn:
				    imgKey = "imgCheckedIn";
				    break;
				case ObjectContainerNode.TreeNodeIconType.ICON_NotCurrent:
				    imgKey = "imgNotCurrent";
				    break;
				case ObjectContainerNode.TreeNodeIconType.ICON_NotInDepot:
				    imgKey = "imgNotInDepot";
				    break;
				case ObjectContainerNode.TreeNodeIconType.ICON_CheckedOutOther:
				    imgKey = "imgCheckedOutOther";
				    break;

				case ObjectContainerNode.TreeNodeIconType.ICON_Ignore:
				default:
					return null;
			}

			return Utils.ApplicationInstance.Resources[imgKey];
		}

		/// Do nothing.
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;
		}
		#endregion
	}
}
