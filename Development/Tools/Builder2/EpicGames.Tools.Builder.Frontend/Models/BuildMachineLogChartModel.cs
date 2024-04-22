using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class BuildMachineLogChartModel
    {
        public string MachineName;

        public IEnumerable<SP_GetMachineBuildLogsResult> FailedBuilds;

        public IEnumerable<SP_GetMachineBuildLogsResult> SucceededBuilds;
    }
}