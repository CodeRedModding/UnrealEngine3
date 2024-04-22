/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Imaging;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for PipelineStep_Visual.xaml
	/// </summary>
	public partial class PipelineStep_Visual : UserControl
	{
		public PipelineStep_Visual()
		{
			InitializeComponent();
		}

		void OnActionButtonClicked( object sender, RoutedEventArgs e )
		{
			OnExecute(sender, e);
		}

		/// Execute the profile step.
		void OnExecute( object sender, RoutedEventArgs e )
		{
			int StepIndex = Profile.Pipeline.Steps.IndexOf(PipelineStep);
			Session.Current.QueueExecuteStep( Profile, StepIndex, false );
			mStepComboButton.OnMenuItemAction();
		}

		void OnRunFromHere(object sender, RoutedEventArgs e)
		{
			int StepIndex = Profile.Pipeline.Steps.IndexOf(PipelineStep);
			Session.Current.RunProfile(Profile, StepIndex, Pipeline.ERunOptions.None, null);
		}

		/// For pipeline steps that support cleaning and executing (e.g. recook, rebuild script).
		void OnCleanAndExecute( object sender, RoutedEventArgs e )
		{
			int StepIndex = Profile.Pipeline.Steps.IndexOf(PipelineStep);
			Session.Current.QueueExecuteStep(Profile, StepIndex, true);
			mStepComboButton.OnMenuItemAction();
		}	


		/// The pipeline step that we are visualizing.
		public Pipeline.Step PipelineStep
		{
			get { return (Pipeline.Step)GetValue(PipelineStepProperty); }
			set { SetValue(PipelineStepProperty, value); }
		}
		public static readonly DependencyProperty PipelineStepProperty =
			DependencyProperty.Register("PipelineStep", typeof(Pipeline.Step), typeof(PipelineStep_Visual), new UIPropertyMetadata(null));

		
		/// The profile whose pipeline contains this pipeline step.
		public Profile Profile
		{
			get { return (Profile)GetValue(ProfileProperty); }
			set { SetValue(ProfileProperty, value); }
		}
		public static readonly DependencyProperty ProfileProperty =
			DependencyProperty.Register("Profile", typeof(Profile), typeof(PipelineStep_Visual), new UIPropertyMetadata(null));

		public string ExecuteDesc
		{
			get { return (string)GetValue(ExecuteDescProperty); }
			set { SetValue(ExecuteDescProperty, value);  }
		}
		public static readonly DependencyProperty ExecuteDescProperty =
			DependencyProperty.Register("ExecuteDesc", typeof(string), typeof(PipelineStep_Visual), new UIPropertyMetadata(null));

		public string CleanAndExecuteDesc
		{
			get { return (string)GetValue(CleanAndExecuteDescProperty); }
			set { SetValue(CleanAndExecuteDescProperty, value); }
		}
		public static readonly DependencyProperty CleanAndExecuteDescProperty =
			DependencyProperty.Register("CleanAndExecuteDesc", typeof(string), typeof(PipelineStep_Visual), new UIPropertyMetadata(null));


		/// The icon representing this pipeline step.
		public BitmapImage Icon
		{
			get { return (BitmapImage)GetValue(IconProperty); }
			set { SetValue(IconProperty, value); }
		}
		public static readonly DependencyProperty IconProperty =
			DependencyProperty.Register("Icon", typeof(BitmapImage), typeof(PipelineStep_Visual), new UIPropertyMetadata(null));

		

	}
}
