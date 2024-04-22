using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace MacPackager
{
	class DeployTime
	{
		static public bool ExecuteDeployCommand(string Command, string RPCCommand)
		{
			switch (Command.ToLowerInvariant())
			{
				case "deploy":
					{
						string ZipPath = Path.GetFullPath(Config.PCStagingRootDir + @"\" + Config.BundleName + ".app.zip");
						string TargetPath = Path.GetFullPath(Path.Combine(Config.DeployPath, Config.BundleName + "-Mac-" + Program.GameConfiguration + ".app.zip"));
						File.Copy(ZipPath, TargetPath, true);
						return true;
					}

				default:
					return false;
			}
		}
	}
}
