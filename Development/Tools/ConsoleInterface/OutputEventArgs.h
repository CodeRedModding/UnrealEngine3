/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::Drawing;

namespace ConsoleInterface
{
	public ref class OutputEventArgs : public EventArgs
	{
	private:
		String ^mMessage;
		Color mTxtColor;

	public:
		property String^ Message
		{
			String^ get();
		}

		property Color TextColor
		{
			Color get();
		}

		OutputEventArgs(Color TxtColor, String ^Msg);
	};
}
