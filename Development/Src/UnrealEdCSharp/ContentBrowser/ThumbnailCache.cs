/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Media;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows;
using System.Globalization;

namespace UnrealEdCSharp.ContentBrowser
{

	/// Thumbnail Cache is a Singleton that stores the default thumbnails to be used to visualize assets that lack dedicated
	/// thumbnails. Requesting a type that is not in the ThumbnailCache will create a new shared thumbnail for that type.
	class ThumbnailCache
	{
		/// ThumbnailCache is a singleton. NOT THREAD SAFE!
		private static ThumbnailCache mInst = null;
		public static ThumbnailCache Get()
		{
			if ( mInst != null )
			{
				return mInst;
			}

			return mInst = new ThumbnailCache();
		}



		/// Holds a map of default thumbnails to images.
		private Dictionary<String, BitmapSource> mThumbnails = new Dictionary<string, BitmapSource>();
		private BitmapImage SharedThumbBackground = null;

		private ThumbnailCache()
		{
			// The background to be used for all shared thumbnails
			SharedThumbBackground = (BitmapImage)Application.Current.Resources["imgSharedThumbBackground"];
		}

		/// <summary>
		/// Get the thumbnail for the type. The name of the type is specified by 'TypeString'.
		/// If there is no thumbnail for a given type then a newly generated one is returned.
		/// </summary>
		/// <param name="TypeString">The name of the type for which to find the thumbnail.</param>
		/// <param name="bIsArchetype">True if the object is an Archetype</param>
		/// <param name="ThumbColor">The color that the thumbnail should be if we have to generate a new one.</param>
		/// <returns>The thumbnail for this type.</returns>
		public ImageSource GetThumbnailForType(String TypeString, bool bIsArchetype, Color ThumbColor)
		{
			BitmapSource Thumbnail = null;
			if (mThumbnails.TryGetValue(TypeString, out Thumbnail))
			{
				return Thumbnail;
			}
			else if(bIsArchetype && mThumbnails.TryGetValue("Archetype", out Thumbnail))
			{
				return Thumbnail;
			}
			else
			{
				// No thumbnail could be found, generate and cache one for this type
				return CreateThumbnailForAssetType(TypeString, bIsArchetype, ThumbColor);
			}
		}

		/// <summary>
		/// Creates and caches a shared thumbnail for the passed in asset type specified by 'TypeString'.
		/// </summary>
		/// <param name="TypeString">The name of the type that should be rendered on the thumbnail</param>
		/// <param name="bIsArchetype">True if the object is an Archetype</param>
		/// <param name="ThumbColor">The color the thumbnail should be</param>
		/// <returns>The thumbnail for this type.</returns>
		private BitmapSource CreateThumbnailForAssetType(String TypeString, bool bIsArchetype, Color ThumbColor)
		{
			// The width and height of the bitmap will be the same as the background we are rendering over.
			int BitmapWidth = SharedThumbBackground.PixelWidth;
			int BitmapHeight = SharedThumbBackground.PixelHeight;
			int BytesPerPixel = SharedThumbBackground.Format.BitsPerPixel / 8;
			int Stride = BitmapWidth * BytesPerPixel;

			// Create the pixel data array and get the pixel data so we can blend the background with the thumbnail color
			Byte[] PixelData = new Byte[BitmapHeight * Stride];
			SharedThumbBackground.CopyPixels(PixelData, Stride, 0);

			// Divide once for converting Bytes to their float equivalents.  
			float Scale = 1.0f / 255.0f;

			// Determine the Hue associated with this Asset Type's color.
			float[] AssetTypeRGB = { ThumbColor.R * Scale, ThumbColor.G * Scale, ThumbColor.B * Scale, ThumbColor.A * Scale };
			float[] AssetTypeHSV = {0,0,0};
			UnrealEd.ColorUtil.RGBToHSV(
				AssetTypeRGB[0], AssetTypeRGB[1], AssetTypeRGB[2],
				out AssetTypeHSV[0], out AssetTypeHSV[1], out AssetTypeHSV[2] );
			float AssetTypeHueOffset = AssetTypeHSV[0];

			// Go through each pixel and blend the background with the thumbnail color
			for (int X = 0; X < BitmapWidth; ++X)
			{
				for (int Y = 0; Y < BitmapHeight; ++Y)
				{
					// Lookup the index of this pixel
					int PixelIdx = ((Y * BitmapWidth) + X) * BytesPerPixel;

					// Get each color and convert it to a float as they are easier to work with for blending
					float B = PixelData[PixelIdx] * Scale;
					float G = PixelData[PixelIdx + 1] * Scale;
					float R = PixelData[PixelIdx + 2] * Scale;
					float A = PixelData[PixelIdx + 3] * Scale;

					// Shift the hue in the direction of the desired thumbnail color.
					float H, S, V;
					UnrealEd.ColorUtil.RGBToHSV( R, G, B, out H, out S, out V );
					H = (H + AssetTypeHueOffset) % 360;
					UnrealEd.ColorUtil.HSVtoRGB( H, S, V, out R, out G, out B );
					
					// Assign blended color back to the same location in the source pixel data
					PixelData[PixelIdx] = (Byte)(B * 255.0f);
					PixelData[PixelIdx + 1] = (Byte)(G * 255.0f);
					PixelData[PixelIdx + 2] = (Byte)(R * 255.0f);
					PixelData[PixelIdx + 3] = (Byte)(A * 255.0f);

				}
			}

			String ArchetypeString = "Archetype";

			String StringToRender;
			if( bIsArchetype )
			{
				// If the type is an archetype the string will always display "archetype".
				StringToRender = String.Copy(ArchetypeString);
			}
			else
			{
				// Copy the string as it might be modified
				StringToRender = String.Copy(TypeString);
			}

            // Make the name of the resource more user friendly
            if (StringToRender.StartsWith("Substance"))
            {
                StringToRender = StringToRender.Replace("Air", "");
            }

			// Format the text so it fits nicely in the thumbnail
			String FontName = "Tahoma";
			double DesiredFontSize = 40;
			FormattedText RenderText = GetFormattedSharedThumbnailText(StringToRender, FontName, DesiredFontSize, BitmapWidth, BitmapHeight);

			// Position the text block in the middle of the thumbnail
			double TextPosY = BitmapWidth * 0.5 - RenderText.Height * 0.5f;

			// Create a new bitmap from the pixel data that was modified.
			BitmapSource Bitmap = BitmapSource.Create(BitmapWidth, BitmapHeight, 96, 96, PixelFormats.Bgra32, null, PixelData, Stride);

			// Create a drawing visual and drawing context so we can render
			DrawingVisual DrawVisual = new DrawingVisual();
			DrawingContext DrawContext = DrawVisual.RenderOpen();

			// Draw underlying image
			DrawContext.DrawImage(Bitmap, new Rect(0, 0, BitmapWidth, BitmapHeight));

			// Create a Radial Gradient Brush moving from Opaque white to Transparent
			GradientStopCollection GradientCollection = new GradientStopCollection();
			GradientCollection.Add(new GradientStop(Colors.White, 0.0));
			GradientCollection.Add(new GradientStop(Colors.Transparent, 0.95));
			RadialGradientBrush RadialBrush = new RadialGradientBrush(GradientCollection);
			RadialBrush.Opacity = 0.4;
			RadialBrush.Freeze();

			// Draw a background glow effect
			DrawContext.DrawEllipse(RadialBrush, null, new Point(BitmapWidth * 0.5, BitmapHeight * 0.5), RenderText.Width * 0.85, RenderText.Height * 0.85);

			// Create a white drop shadow effect on the text
			DrawContext.PushOpacity(0.4);
			{
				RenderText.SetForegroundBrush(Brushes.White);
				DrawContext.DrawText(RenderText, new Point(1, TextPosY+1));			
			}
			DrawContext.Pop();

			// And finally draw our text
			DrawContext.PushOpacity(0.9);
			{
				RenderText.SetForegroundBrush(Brushes.Black);
				DrawContext.DrawText(RenderText, new Point(0, TextPosY));
			}
			DrawContext.Pop();

			// Close the drawing context
			DrawContext.Close();

			// Render everything to a bitmap
			RenderTargetBitmap RenderTargetBMP = new RenderTargetBitmap(BitmapWidth, BitmapHeight, 96, 96, PixelFormats.Pbgra32);
			RenderTargetBMP.Render(DrawVisual);

			// Cache the bitmap so we only create it once
			if (bIsArchetype)
			{
				mThumbnails.Add(ArchetypeString, RenderTargetBMP);
			}
			else
			{
				mThumbnails.Add(TypeString, RenderTargetBMP);
			}

			return RenderTargetBMP;
		}

		/// <summary>
		/// Formats the passed in text so that the resulting text fits nicely in the thumbnail area
		/// </summary>
		/// <param name="Text">The text to format</param>
		/// <param name="FontName">The name of the font to use</param>
		/// <param name="DesiredFontSize">The desired font size.  Note: If the text can't be placed on new lines, the font size may decrease in order to fit the text on the thumbnail</param>
		/// <param name="BitmapWidth">The width of the bitmap</param>
		/// <param name="BitmapHeight">The height of the bitmap</param>
		/// <returns>A formatted text object for the text</returns>
		private FormattedText GetFormattedSharedThumbnailText(String Text, String FontName, double DesiredFontSize, int BitmapWidth, int BitmapHeight)
		{
			Typeface typeface = new Typeface(new FontFamily(FontName),
											 FontStyles.Normal,
											 FontWeights.Bold,
											 FontStretches.Normal);

			// Get the GlyphTypeface which has information on the size of each character for the chosen font
			GlyphTypeface GlyphInfo;
			typeface.TryGetGlyphTypeface(out GlyphInfo);

			// The font size we are using, initially the desired font size.
			double FontSize = DesiredFontSize;

			// The padding that should be added so the text isn't right over the thumbnail edge.
			double Padding = 15.0f;

			// always check the size of the string once
			bool bCheckStringSize = true;

			// the total width of the current line in the string
			double TotalLineWidth = 0.0;

			while (bCheckStringSize)
			{
				// Begin by assuming all text will fit nicely the first pass
				bCheckStringSize = false;

				TotalLineWidth = 0.0;

				// Go through each char in the string and sum line width
				for (int CharIdx = 0; CharIdx < Text.Length; ++CharIdx)
				{
					char CurrentChar = Text[CharIdx];
					if (CurrentChar == '\n')
					{
						//Start over if we found a new line
						TotalLineWidth = 0.0;
					}
					else if (!Char.IsControl(CurrentChar))
					{
						// Only sum width if we aren't working with control characters like line feeds, and junk like that
						// Get the width of the char from the glyph info
						ushort GlyphIndex = GlyphInfo.CharacterToGlyphMap[CurrentChar];
						double CharWidth = GlyphInfo.AdvanceWidths[GlyphIndex] * FontSize;
						TotalLineWidth += CharWidth;

						// If the total line width exceeds the maximum space we allow for text on one line...
						if (TotalLineWidth > BitmapWidth - Padding)
						{
							bool bFoundWord = false;
							// ...find the last "Word" in the string and place it on the next line
							// Do this by searching backwards for an uppercase letter (Note: spaces in the string will be automatically formatted)
							for (int RevCharIdx = Text.Length - 1; RevCharIdx >= 0; --RevCharIdx)
							{
								// Don't check the first character, or capital letters with new line characters in front of them.
								if (RevCharIdx > 0 && Char.IsUpper(Text[RevCharIdx]) && Text[RevCharIdx - 1] != '\n')
								{
									//Insert a new line
									Text = Text.Insert(RevCharIdx, System.Environment.NewLine);
									bFoundWord = true;
									break;
								}
							}
							
							if( !bFoundWord )
							{
								// if we didnt find a word and the text is still out of bounds, reduce the font size
								if (FontSize > 5.0)
								{
									// reduce the font size and recheck the string
									FontSize -= 5.0;
									bCheckStringSize = true;
								}
							}
							else
							{
								// if we found a word and placed it on a new line, recheck again to make sure all lines are within the bitmap boundaries.
								bCheckStringSize = true;
							}
					
							// Break out of the main character search loop, we have to start over anyway
							break;
						}
					}

				}
			}

			// Return a new formatted text object with our font size and formatted text
			FormattedText FT = new FormattedText(	Text,
													new CultureInfo("en-us"),
													FlowDirection.LeftToRight,
													typeface,
													FontSize,
													Brushes.Black);

			FT.MaxTextWidth = BitmapWidth;
			FT.MaxTextHeight = BitmapHeight;
			FT.TextAlignment = TextAlignment.Center;

			return FT;

		}
	}
}
