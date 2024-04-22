using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class VerifyStatusModel
    {
        /// <summary>
        /// Holds the collection of branches.
        /// </summary>

        public IEnumerable<SP_GetVerificationBranchesResult> Branches;
    }
}