/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;


namespace ConsoleInterface {

	/// <summary>
	/// Summary for ConnectToConsoleRetryForm
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class ConnectToConsoleRetryForm : public System::Windows::Forms::Form
	{
	public:
		property String^ Message
		{
			void set(String ^Value)
			{
				if(Value == nullptr)
				{
					throw gcnew ArgumentNullException(L"Value");
				}

				mDisplayMessage->Text = Value;
			}
		}

	public:
		ConnectToConsoleRetryForm(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//

			mIcon->Image = SystemIcons::Exclamation->ToBitmap();
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~ConnectToConsoleRetryForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private: System::Windows::Forms::PictureBox^  mIcon;
	protected: 

	private: System::Windows::Forms::Button^  mButtonCancel;
	protected: 

	private: System::Windows::Forms::Button^  mButtonRetry;
	private: System::Windows::Forms::Label^  mDisplayMessage;



	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->mIcon = (gcnew System::Windows::Forms::PictureBox());
			this->mButtonCancel = (gcnew System::Windows::Forms::Button());
			this->mButtonRetry = (gcnew System::Windows::Forms::Button());
			this->mDisplayMessage = (gcnew System::Windows::Forms::Label());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->mIcon))->BeginInit();
			this->SuspendLayout();
			// 
			// mIcon
			// 
			this->mIcon->BackgroundImageLayout = System::Windows::Forms::ImageLayout::Stretch;
			this->mIcon->Location = System::Drawing::Point(12, 12);
			this->mIcon->Name = L"mIcon";
			this->mIcon->Size = System::Drawing::Size(32, 32);
			this->mIcon->TabIndex = 0;
			this->mIcon->TabStop = false;
			// 
			// mButtonCancel
			// 
			this->mButtonCancel->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->mButtonCancel->DialogResult = System::Windows::Forms::DialogResult::Cancel;
			this->mButtonCancel->Location = System::Drawing::Point(293, 73);
			this->mButtonCancel->Name = L"mButtonCancel";
			this->mButtonCancel->Size = System::Drawing::Size(75, 23);
			this->mButtonCancel->TabIndex = 1;
			this->mButtonCancel->Text = L"&Cancel";
			this->mButtonCancel->UseVisualStyleBackColor = true;
			// 
			// mButtonRetry
			// 
			this->mButtonRetry->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->mButtonRetry->DialogResult = System::Windows::Forms::DialogResult::Retry;
			this->mButtonRetry->Location = System::Drawing::Point(212, 73);
			this->mButtonRetry->Name = L"mButtonRetry";
			this->mButtonRetry->Size = System::Drawing::Size(75, 23);
			this->mButtonRetry->TabIndex = 0;
			this->mButtonRetry->Text = L"&Retry";
			this->mButtonRetry->UseVisualStyleBackColor = true;
			// 
			// mDisplayMessage
			// 
			this->mDisplayMessage->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom) 
				| System::Windows::Forms::AnchorStyles::Left) 
				| System::Windows::Forms::AnchorStyles::Right));
			this->mDisplayMessage->Location = System::Drawing::Point(50, 12);
			this->mDisplayMessage->Name = L"mDisplayMessage";
			this->mDisplayMessage->Size = System::Drawing::Size(318, 58);
			this->mDisplayMessage->TabIndex = 2;
			this->mDisplayMessage->Text = L"label1";
			// 
			// ConnectToConsoleRetryForm
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(380, 108);
			this->ControlBox = false;
			this->Controls->Add(this->mDisplayMessage);
			this->Controls->Add(this->mButtonRetry);
			this->Controls->Add(this->mButtonCancel);
			this->Controls->Add(this->mIcon);
			this->MaximizeBox = false;
			this->MinimizeBox = false;
			this->Name = L"ConnectToConsoleRetryForm";
			this->ShowIcon = false;
			this->SizeGripStyle = System::Windows::Forms::SizeGripStyle::Hide;
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterParent;
			this->Text = L"ConnectToConsoleRetryForm";
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->mIcon))->EndInit();
			this->ResumeLayout(false);

		}
#pragma endregion
	};
}
