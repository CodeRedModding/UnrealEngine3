using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class LatestStatusModel
    {
        /// <summary>
        /// Holds the latest successful builds per branch.
        /// </summary>

        public IEnumerable<SP_GetLatestBuildsResult> LatestBuilds;
    }
}