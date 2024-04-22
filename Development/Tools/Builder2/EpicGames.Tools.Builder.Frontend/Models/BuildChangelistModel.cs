using System;
using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;


namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class BuildChangelistModel
    {
        /// <summary>
        /// Holds the changelist's collection of build log items.
        /// </summary>

        public IEnumerable<SP_GetChangelistBuildLogsResult> BuildLogs;

        /// <summary>
        /// Holds the changelist's identifier.
        /// </summary>

        public int ChangelistId;

        /// <summary>
        /// Holds the changelist's collection of CIS log items.
        /// </summary>

        public IEnumerable<SP_GetChangelistCisLogsResult> CisLogs;

        /// <summary>
        /// Holds the changelist details.
        /// </summary>

        public SP_GetChangelistDetailsResult Details;
    }
}