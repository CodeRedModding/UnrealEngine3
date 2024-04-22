using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;


namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class BuildStatusModel
    {
        /// <summary>
        /// Holds the collection of branches.
        /// </summary>

        public IEnumerable<SP_GetBuildBranchesResult> Branches;
    }
}