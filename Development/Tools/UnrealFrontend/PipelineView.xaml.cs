/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Collections.Generic;
using System.Windows.Media.Imaging;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for PipelineView.xaml
	/// </summary>
	public partial class PipelineView : UserControl
	{
		public PipelineView()
		{
			InitializeComponent();
		}

		/// The currenly selected profile.
		public Profile Profile
		{
			get { return (Profile)GetValue(ProfileProperty); }
			set { SetValue(ProfileProperty, value); }
		}
		public static readonly DependencyProperty ProfileProperty =
			DependencyProperty.Register("Profile", typeof(Profile), typeof(PipelineView), new UIPropertyMetadata(null));

		
		/// A reference to the Pipeline that this PipelineView will modify
		public Pipeline.UFEPipeline Pipeline
		{
			get { return (Pipeline.UFEPipeline)GetValue(ProcessProperty); }
			set { SetValue(ProcessProperty, value); }
		}
		public static readonly DependencyProperty ProcessProperty =
			DependencyProperty.Register("Pipeline", typeof(Pipeline.UFEPipeline), typeof(PipelineView), new UIPropertyMetadata(OnPipelineChanged));

		static void OnPipelineChanged(object sender, DependencyPropertyChangedEventArgs e)
		{
			PipelineView This = (PipelineView)sender;
			This.RecreateVisuals();
		}

		/// A convenience property that is true when the app is busy.
		/// When the app is busy the blue throbber is spinning.
		public bool IsWorking
		{
			get { return (bool)GetValue(IsWorkingProperty); }
			set { SetValue(IsWorkingProperty, value); }
		}
		public static readonly DependencyProperty IsWorkingProperty =
			DependencyProperty.Register("IsWorking", typeof(bool), typeof(PipelineView), new UIPropertyMetadata(null));

		/// Clear all the old visuals and recreate them.
		private void RecreateVisuals()
		{
			this.mStepsContainer.Children.Clear();

			if (Pipeline != null)
			{
				// Generate a control for each pipeline steps.
				foreach (Pipeline.Step SomeStep in Pipeline.Steps)
				{
					mStepsContainer.Children.Add(CreatePipelineStepVisualizer(SomeStep, Pipeline));
				}
			}
		}

		/// Generate a control that represents a step in the pipeline
		private static FrameworkElement CreatePipelineStepVisualizer( Pipeline.Step VisualizeMe, UnrealFrontend.Pipeline.UFEPipeline InPipeline )
		{
			System.Collections.Generic.Dictionary<Type, String> StepIconMapping = new System.Collections.Generic.Dictionary<Type, String>
			{
				{ typeof(Pipeline.MakeScript), "MakeScript_png" },
				{ typeof(Pipeline.Cook), "Cook_png" },
				{ typeof(Pipeline.Sync), "Sync_png" },
				{ typeof(Pipeline.SyncAndroid), "SyncMobile_png" },
                { typeof(Pipeline.PackageAndroid), "Package_png" },
				{ typeof(Pipeline.RebootAndSync), "Sync_png" },
				{ typeof(Pipeline.Launch), "Launch_png" },
				{ typeof(Pipeline.DeployIOS), "SyncMobile_png" },
				{ typeof(Pipeline.PackageIOS), "Package_png" },
				{ typeof(Pipeline.DeployMac), "Launch_png" },
				{ typeof(Pipeline.PackageMac), "Package_png" },
				{ typeof(Pipeline.PackageFlash), "Package_png" },
				{ typeof(Pipeline.UnProp), "UnProp_png" },
				{ typeof(Pipeline.UnSetup), "Package_png" },
			};


			String StepIconKey = null;
			StepIconMapping.TryGetValue(VisualizeMe.GetType(), out StepIconKey);

			return new PipelineStep_Visual
			{
				PipelineStep = VisualizeMe,
				Profile = InPipeline.OwnerProfile,
				Icon = Application.Current.Resources[ StepIconKey==null ? "Step_missing_icon_png" : StepIconKey ] as BitmapImage,
				ExecuteDesc = VisualizeMe.ExecuteDesc,
				CleanAndExecuteDesc = VisualizeMe.CleanAndExecuteDesc,
			};

		}
	}
	
}