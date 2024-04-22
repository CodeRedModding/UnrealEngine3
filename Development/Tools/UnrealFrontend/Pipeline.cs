/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;
using System.Xml.Serialization;

namespace UnrealFrontend.Pipeline
{

	public enum ERunOptions
	{
		// Accessed as a bitmask
		None = 0x01,
		RebuildScript = 0x02,
		FullReCook = 0x04,
		CookINIsOnly = 0x08,
	}

	[XmlInclude(typeof(Step))]
	[XmlInclude(typeof(CommandletStep))]

	[XmlInclude(typeof(MakeScript))]
	[XmlInclude(typeof(Cook))]
	[XmlInclude(typeof(Sync))]
	[XmlInclude(typeof(SyncAndroid))]
	[XmlInclude(typeof(PackageFlash))]
	[XmlInclude(typeof(RebootAndSync))]
	[XmlInclude(typeof(Launch))]
	[XmlInclude(typeof(PackageIOS))]
	[XmlInclude(typeof(DeployIOS))]
	[XmlInclude(typeof(PackageMac))]
	[XmlInclude(typeof(UnSetup))]
	[XmlInclude(typeof(UnProp))]
    [XmlInclude(typeof(PackageAndroid))]

	public class UFEPipeline
	{
		/// Given a profile, return a list of steps that represents the pipeline for this profile.
		public static UFEPipeline GetPipelineFor( Profile InProfile )
		{
			UFEPipeline CurrentPipeline = InProfile.Pipeline;

			switch (InProfile.TargetPlatformType )
			{
				default:
				case ConsoleInterface.PlatformType.PCConsole:
				case ConsoleInterface.PlatformType.PCServer:
				case ConsoleInterface.PlatformType.PC:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new Launch(),
							new UnSetup() as Pipeline.Step,
						}
					);
				case ConsoleInterface.PlatformType.Xbox360:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new RebootAndSync(),
							new Launch(),
							new UnProp(),
						}
					);

				case ConsoleInterface.PlatformType.PS3:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new Launch(),
							new UnProp(),
						}
					);

				case ConsoleInterface.PlatformType.WiiU:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new Sync(),
							new Launch(),
						}
					);

				case ConsoleInterface.PlatformType.IPhone:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new PackageIOS(),
							new DeployIOS(),
						}
					);

				case ConsoleInterface.PlatformType.Android:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
                            new PackageAndroid(),
							new SyncAndroid(),
							new Launch(),
						}
					);

				case ConsoleInterface.PlatformType.Flash:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new PackageFlash(),
                            new Launch(),
						}
					);

				case ConsoleInterface.PlatformType.NGP:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new Launch(),
							new UnProp(),
						}
					);

				case ConsoleInterface.PlatformType.MacOSX:
					return new UFEPipeline(InProfile, new List<Pipeline.Step>
						{
							new MakeScript(),
							new Cook(),
							new PackageMac(),
						}
					);
			}
		}

		/// Given a profile and a pipeline check that the pipeline is appropriate for this profile.
		/// If the pipeline is appropriate, return it; otherwise return a new pipeline that is valid.
		public static UFEPipeline ValidatePipeline( Profile InProfile, UFEPipeline PipelineToValidate )
		{
			UFEPipeline ExpectedPipeline = GetPipelineFor(InProfile);

			// A valid pipeline has the correct number of correct steps.
			bool bPipelineOK = (PipelineToValidate.Steps.Count == ExpectedPipeline.Steps.Count);
			for ( int StepIndex = 0; bPipelineOK && StepIndex < ExpectedPipeline.Steps.Count; ++StepIndex)
			{
				bPipelineOK = ExpectedPipeline.Steps[StepIndex].GetType() == PipelineToValidate.Steps[StepIndex].GetType();
			}
			
			if (bPipelineOK)
			{
				// Mark this pipeline as valid and return.
				PipelineToValidate.OwnerProfile = InProfile;
				return PipelineToValidate;
			}
			else
			{
				// Pipeline was invalid; return a valid one instead.
				return ExpectedPipeline;
			}
		}

		public UFEPipeline() : this( null, new List<Pipeline.Step>() )
		{

		}

		private UFEPipeline( Profile InProfile, List<Pipeline.Step> InPipelineSteps )
		{
			OwnerProfile = InProfile;
			TargetPlatformType = (OwnerProfile == null) ? ConsoleInterface.PlatformType.All : OwnerProfile.TargetPlatformType;
			Steps = new List<Pipeline.Step>( InPipelineSteps );
		}
		
		[XmlIgnore]
		public Profile OwnerProfile { get; private set; }

		public ConsoleInterface.PlatformType TargetPlatformType { get; set; }

		/// A list of steps that comprises this pipeline
		public List<Pipeline.Step> Steps { get; set; }
	}
}
