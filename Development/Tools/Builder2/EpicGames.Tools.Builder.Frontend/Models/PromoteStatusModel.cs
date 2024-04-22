using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;


namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class PromoteStatusModel
    {
        /// <summary>
        /// Holds the collection of promotion commands.
        /// </summary>

        public IEnumerable<SP_GetPromotionCommandsResult> PromotionCommands;
    }
}