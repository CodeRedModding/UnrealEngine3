/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections::Generic;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;
using namespace System::Reflection;
using namespace System::IO;

namespace ConsoleInterface {

	/// <summary>
	/// Summary for EnumeratingPlatformsForm
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class EnumeratingPlatformsForm : public System::Windows::Forms::Form
	{
	public:
		EnumeratingPlatformsForm(System::Threading::ManualResetEvent ^LifetimeEvent)
		{
			InitializeComponent();

			//
			//TODO: Add the constructor code here
			//
			this->Text = String::Format(L"{0}: {1}", Path::GetFileNameWithoutExtension(Assembly::GetEntryAssembly()->Location), this->Text);
			this->mAliveEvent = LifetimeEvent;
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~EnumeratingPlatformsForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private: System::Threading::ManualResetEvent ^mAliveEvent;
	private: System::Windows::Forms::Label^  label1;
	private: System::Windows::Forms::ProgressBar^  progressBar1;
	private: System::Windows::Forms::Timer^  mAliveTimer;
	private: System::ComponentModel::IContainer^  components;

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->components = (gcnew System::ComponentModel::Container());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->progressBar1 = (gcnew System::Windows::Forms::ProgressBar());
			this->mAliveTimer = (gcnew System::Windows::Forms::Timer(this->components));
			this->SuspendLayout();
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(12, 9);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(270, 13);
			this->label1->TabIndex = 0;
			this->label1->Text = L"Enumerating all licensed platforms and available targets.";
			// 
			// progressBar1
			// 
			this->progressBar1->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Left) 
				| System::Windows::Forms::AnchorStyles::Right));
			this->progressBar1->Location = System::Drawing::Point(15, 25);
			this->progressBar1->MarqueeAnimationSpeed = 50;
			this->progressBar1->Name = L"progressBar1";
			this->progressBar1->Size = System::Drawing::Size(426, 23);
			this->progressBar1->Style = System::Windows::Forms::ProgressBarStyle::Marquee;
			this->progressBar1->TabIndex = 1;
			// 
			// mAliveTimer
			// 
			this->mAliveTimer->Enabled = true;
			this->mAliveTimer->Interval = 500;
			this->mAliveTimer->Tick += gcnew System::EventHandler(this, &EnumeratingPlatformsForm::mAliveTimer_Tick);
			// 
			// EnumeratingPlatformsForm
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->AutoSize = true;
			this->ClientSize = System::Drawing::Size(453, 59);
			this->ControlBox = false;
			this->Controls->Add(this->progressBar1);
			this->Controls->Add(this->label1);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
			this->MaximizeBox = false;
			this->MinimizeBox = false;
			this->Name = L"EnumeratingPlatformsForm";
			this->ShowInTaskbar = false;
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Text = L"Please be patient";
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
	
	private: 
		System::Void mAliveTimer_Tick( System::Object^ , System::EventArgs^ )
		{
			if( mAliveEvent->WaitOne( 1, false ) )
			{
				this->Close();
			}
		}
	};
}
