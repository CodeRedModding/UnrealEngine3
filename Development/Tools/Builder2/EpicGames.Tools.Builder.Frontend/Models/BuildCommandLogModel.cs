using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;


namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class BuildCommandLogModel
    {
        /// <summary>
        /// Holds the list of build logs for this command.
        /// </summary>

        public IEnumerable<SP_GetCommandBuildLogsResult> BuildLogs;

        /// <summary>
        /// Holds the command's details.
        /// </summary>

        public SP_GetCommandDetailsResult Details;
    }
}