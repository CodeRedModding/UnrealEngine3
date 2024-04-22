#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "SubstanceAirEdGraphInstanceEditorWindowShared.h"
#include "WPFFrameCLR.h"
#include "ThumbnailToolsCLR.h"

#include "ConvertersCLR.h"
#include "ColorPickerShared.h"

using namespace System;
using namespace System::ComponentModel;
using namespace System::Windows::Controls::Primitives;
using namespace System::Windows::Media::Imaging;
using namespace System::Windows::Input;
using namespace System::Deployment;

namespace SubstanceAir
{
	namespace Controls
	{
		static const char * CHANNEL_LABELS[4] = {"R", "G", "B", "A"};

		struct Color4F
		{
			union
			{
				struct
				{
					float mR;
					float mG;
					float mB;
					float mA;
				};

				float mValues[4];
			};

			Color4F(float r=0, float g=0, float b=0, float a=1.0f) :
			mR(r), mG(g), mB(b), mA(a)
			{
			}
		};

		public delegate void ColorChangedHandler(System::Object ^ emitter, Color4F newColor);

		public delegate void PickerMovedHandler(System::Object ^ emitter);

		public ref class ColorWheel : public CustomControls::ColorWheel
		{
		protected:
			virtual void OnMouseMove(System::Windows::Input::MouseEventArgs ^ e) override
			{
				CustomControls::ColorWheel::OnMouseMove(e);
				if (System::Windows::Input::Mouse::Captured != this)
					return;

				PickerMoved(this);
			}

		public:
			event PickerMovedHandler ^ PickerMoved;
		};

		public ref class ColorPicker : public UniformGrid
		{
		private:
			// Channel count - 3 or 4 depending on alpha
			size_t									ChannelCount;

			// Widgets
			array< CustomControls::DragSlider ^ > ^	Sliders;
			Button ^								PickButton;
			Popup ^									PopupContext;
			ColorWheel ^							ColorWheelWidget;
			CustomControls::DragSlider ^			BrightnessSlider;

			MScopedNativePointer<Color4F>			ColorPtr;

			// Used to stop event propagation
			//
			// mSlider->Value = 1.0; <-- This raise an event! 
			// So if you set it in an event callback, it can loop
			// forever!
			bool									mDisableEvents;

			void updateUi()
			{
				// Refresh pick button
				PickButton->Background = gcnew SolidColorBrush(Color::FromArgb(
					(System::Byte)(ChannelCount == 3 ? 255 : ColorPtr->mA * 255.0f),
					(System::Byte)(ColorPtr->mR * 255.0f),
					(System::Byte)(ColorPtr->mG * 255.0f),
					(System::Byte)(ColorPtr->mB * 255.0f)
					));

				// Refresh sliders
				for (size_t i=0; i<ChannelCount; i++)
				{
					Sliders[i]->Value = ColorPtr->mValues[i];
				}

				FLinearColor col(ColorPtr->mR, ColorPtr->mG, ColorPtr->mB, ColorPtr->mA);
				col = col.LinearRGBToHSV();

				ColorWheelWidget->Hue = col.R;
				ColorWheelWidget->Saturation = col.G;
				ColorWheelWidget->Brightness = col.B;
				BrightnessSlider->Value = ColorWheelWidget->Brightness;
			}


			//! @brief Called when a slider is updated
			void sliderChanged()
			{
				for (size_t i=0; i<ChannelCount; i++)
				{
					ColorPtr->mValues[i] = Sliders[i]->Value;
				}
				
				mDisableEvents = true;
				updateUi();
				mDisableEvents = false;

				ColorChanged(this, *ColorPtr.Get());				// Raise event
			}


			void sliderValueChanged(
				System::Object ^ sender, 
				System::Windows::RoutedPropertyChangedEventArgs<double> ^ e)
			{
				if (mDisableEvents)
				{
					return;
				}

				sliderChanged();
				e->Handled = true;
			}


			void sliderMouseUp(
				System::Object ^ sender, 
				System::Windows::Input::MouseButtonEventArgs ^ e)
			{
				if (mDisableEvents)
				{
					return;
				}

				sliderChanged();
			}


			void pickButtonMouseEnter(
				System::Object ^ sender,
				System::Windows::RoutedEventArgs ^ e)
			{
				// Force setup of right color before showing the picker
				updateUi();
				PopupContext->IsOpen = true;
				e->Handled = true;
			}


			void wheelChanged()
			{
				FLinearColor col(
					ColorWheelWidget->Hue,
					ColorWheelWidget->Saturation,
					ColorWheelWidget->Brightness
					);

				col = col.HSVToLinearRGB();
				ColorPtr->mR = col.R;
				ColorPtr->mG = col.G;
				ColorPtr->mB = col.B;

				mDisableEvents = true;
				updateUi();
				mDisableEvents = false;

				ColorChanged(this, *ColorPtr.Get());				// Raise event
			}

			void wheelMove(
				System::Object ^ sender)
			{
				if (mDisableEvents)
				{
					return;
				}

				wheelChanged();
			}

			void wheelClicked(
				System::Object ^ sender,
				System::Windows::Input::MouseButtonEventArgs ^ e)
			{
				if (mDisableEvents)
				{
					return;
				}

				wheelChanged();
				e->Handled = false;
			}

			void brightnessValueChanged(
				System::Object ^ sender, 
				System::Windows::RoutedPropertyChangedEventArgs<double> ^ e)
			{
				if (mDisableEvents)
				{
					return;
				}

				ColorWheelWidget->Brightness = e->NewValue;
				wheelChanged();
				e->Handled = true;
			}

		public:

			ColorPicker(bool alpha, const Color4F & initialColor) : 
				ChannelCount(alpha ? 4 : 3), mDisableEvents(false)
			{
				ColorPtr.Reset(new Color4F(initialColor));

				// Setup grid
				Columns = ChannelCount + 1;
				Rows = 1;

				Sliders = gcnew array<CustomControls::DragSlider^>(ChannelCount);

				// Create sliders
				for (size_t i=0; i<ChannelCount; i++)
				{
					CustomControls::DragSlider^ slider = gcnew CustomControls::DragSlider();
					slider->Height = 18;
					slider->Margin = System::Windows::Thickness(0, 0, 5, 0);
					slider->Minimum = 0.0; slider->SliderMin = 0.0;
					slider->Maximum = 1.0; slider->SliderMax = 1.0;
					slider->ValuesPerDragPixel = 0.01;
					slider->DrawAsPercentage = false;
					slider->Name = gcnew String(CHANNEL_LABELS[i]);
					Sliders[i] = slider;
					Children->Add(slider);
				}
				
				// Link sliders to get tab navigation
				for (size_t i=0; i<ChannelCount; i++)
					Sliders[i]->NextDragSliderControl = Sliders[(i+1) % ChannelCount];

				// Create color picker popup window
				ColorWheelWidget = gcnew ColorWheel();

				// Color pick button
				PickButton = gcnew Button();
				PickButton->Height = 20;

				Image ^ pickButtonImage = gcnew Image();
				pickButtonImage->Source = (BitmapImage ^)
					ColorWheelWidget->FindResource("imgEyeDropper");
				PickButton->Content = pickButtonImage;
				Children->Add(PickButton);

				// Brightness slider
				BrightnessSlider = gcnew CustomControls::DragSlider();
				BrightnessSlider->SliderMin = 0.0f;
				BrightnessSlider->SliderMax = 1.0f;
				BrightnessSlider->ValuesPerDragPixel = 0.01;
				BrightnessSlider->Name = "Brightness";

				StackPanel ^ Stack = gcnew StackPanel;
				Stack->Children->Add(ColorWheelWidget);
				Stack->Children->Add(BrightnessSlider);

				PopupContext = gcnew Popup();
				PopupContext->Child = Stack;
				PopupContext->AllowsTransparency = false;
				PopupContext->IsOpen = false;
				PopupContext->StaysOpen = false;
				PopupContext->Placement = 
					System::Windows::Controls::Primitives::PlacementMode::MousePoint;
				
				// Make all widgets match the current color
				updateUi();
				
				// Should be done once UI has been updated
				for (size_t i=0; i<ChannelCount; i++)
				{
					Sliders[i]->ValueChanged +=
						gcnew System::Windows::RoutedPropertyChangedEventHandler<double>(
							this, &ColorPicker::sliderValueChanged);
					Sliders[i]->MouseUp +=
						gcnew System::Windows::Input::MouseButtonEventHandler(
							this, &ColorPicker::sliderMouseUp);
				}

				PickButton->Click += gcnew System::Windows::RoutedEventHandler(
					this, &ColorPicker::pickButtonMouseEnter);

				// Color wheel events
				BrightnessSlider->ValueChanged +=
					gcnew System::Windows::RoutedPropertyChangedEventHandler<double>(
						this, &ColorPicker::brightnessValueChanged);
				
				ColorWheelWidget->MouseLeftButtonDown += 
					gcnew System::Windows::Input::MouseButtonEventHandler(
						this, &ColorPicker::wheelClicked);
				ColorWheelWidget->MouseLeftButtonUp += 
					gcnew System::Windows::Input::MouseButtonEventHandler(
						this, &ColorPicker::wheelClicked);
				ColorWheelWidget->PickerMoved += gcnew PickerMovedHandler(
					this, &ColorPicker::wheelMove);
			}

			size_t channelCount()
			{
				return ChannelCount;
			}

			// Color changed event
			event ColorChangedHandler ^ ColorChanged;
		};
	}
}
