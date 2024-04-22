// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

using Controller;
using Controller.Models;

namespace Controller
{
	public partial class SandboxedAction
	{
		public MODES GetAllTargets()
		{
			return MODES.Finalise;
		}
	}
}
