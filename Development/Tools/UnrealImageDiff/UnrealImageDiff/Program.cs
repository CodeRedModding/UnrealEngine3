/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Text;

namespace UnrealImageDiff
{
	class Program
	{
		static int GetComponentDiff( int AlphaComponent, int BetaComponent, int Threshold )
		{
			int Component = Math.Abs( AlphaComponent - BetaComponent );
			if( Component < Threshold )
			{
				Component = 0;
			}

			return Component;
		}

		static Bitmap GetDiffImage( Bitmap Alpha, Bitmap Beta, int Threshold )
		{
			int Width = Math.Min( Alpha.Width, Beta.Width );
			int Height = Math.Min( Alpha.Height, Beta.Height );

			Bitmap Gamma = new Bitmap( Width, Height, PixelFormat.Format24bppRgb );

			for( int X = 0; X < Width; X++ )
			{
				for( int Y = 0; Y < Height; Y++ )
				{
					Color AlphaColor = Alpha.GetPixel( X, Y );
					Color BetaColor = Beta.GetPixel( X, Y );

					int DiffR = GetComponentDiff( AlphaColor.R, BetaColor.R, Threshold );
					int DiffG = GetComponentDiff( AlphaColor.G, BetaColor.G, Threshold );
					int DiffB = GetComponentDiff( AlphaColor.B, BetaColor.B, Threshold );

					Color NewColor = Color.FromArgb( DiffR, DiffG, DiffB );
					Gamma.SetPixel( X, Y, NewColor );
				}
			}

			return Gamma;
		}

		static void Despeckle( ref Bitmap Gamma )
		{
			for( int X = 1; X < Gamma.Width - 1; X++ )
			{
				for( int Y = 1; Y < Gamma.Height - 1; Y++ )
				{
					Color SpeckleColor = Gamma.GetPixel( X, Y );
					if( SpeckleColor.ToArgb() != Color.Black.ToArgb() )
					{
						if( Gamma.GetPixel( X, Y - 1 ).ToArgb() == Color.Black.ToArgb() )
						{
							if( Gamma.GetPixel( X, Y + 1 ).ToArgb() == Color.Black.ToArgb() )
							{
								if( Gamma.GetPixel( X - 1, Y ).ToArgb() == Color.Black.ToArgb() )
								{
									if( Gamma.GetPixel( X + 1, Y ).ToArgb() == Color.Black.ToArgb() )
									{
										Gamma.SetPixel( X, Y, Color.Black );
									}
								}
							}
						}
					}
				}
			}
		}

		static float GetErrorMetric( Bitmap Gamma )
		{
			float Metric = 0;
			for( int X = 0; X < Gamma.Width; X++ )
			{
				for( int Y = 0; Y < Gamma.Height; Y++ )
				{
					Color Color = Gamma.GetPixel( X, Y );
					Metric += Color.R + Color.G + Color.B;
				}
			}

			return Metric / ( Gamma.Width * Gamma.Height );
		}

		static int Main( string[] Args )
		{
			if( Args.Length < 4 )
			{
				Console.WriteLine( "UID ERROR: Not enough parameters" );
				Console.WriteLine( "UnrealImageDiff Threshold ReferenceImage NewImage DifferenceImage" );
				return -1;
			}

			int Threshold = 1;
			if( !Int32.TryParse( Args[0], out Threshold ) )
			{
				Console.WriteLine( "UID ERROR: Failed to parse threshold parameter." );
				return -2;
			}

			Console.WriteLine( "Comparing image '" + Args[1] + "' with image '" + Args[2] + "' with a threshold of " + Threshold.ToString() );

			Console.WriteLine( " ... loading image: '" + Args[1] + "'" );
			Bitmap Alpha = new Bitmap( Args[1] );
			if( Alpha == null )
			{
				Console.WriteLine( "UID ERROR: Failed to load image '" + Args[0] + "'" );
				return -3;
			}

			Console.WriteLine( " ... loading image: '" + Args[2] + "'" );
			Bitmap Beta = new Bitmap( Args[2] );
			if( Beta == null )
			{
				Console.WriteLine( "UID ERROR: Failed to load image '" + Args[1] + "'" );
				return -3;
			}

			Console.WriteLine( " ... calculating image difference." );
			Bitmap Gamma = GetDiffImage( Alpha, Beta, Threshold );

			Console.WriteLine( " ... despeckling image." );
			Despeckle( ref Gamma );

			Console.WriteLine( " ... saving image to '" + Args[3] + "'" );
			Gamma.Save( Args[3], ImageFormat.Png );

			float ErrorMetric = GetErrorMetric( Gamma );
			Console.WriteLine( " ... arbitrary error: " + ErrorMetric.ToString() );

			if( ErrorMetric < 1.0f )
			{
				Console.WriteLine( "Images are the same!" );
			}
			else
			{
				Console.WriteLine( "Images are different!" );
			}

			return 0;
		}
	}
}
