using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class PromoteCommandModel
    {
        /// <summary>
        /// Holds the command's details.
        /// </summary>

        public SP_GetCommandDetailsResult Details;

        /// <summary>
        /// Holds the currently promoted build label.
        /// </summary>

        public SP_GetPromotedLabelResult PromotedLabel;

        /// <summary>
        /// Holds a flag indicating whether the current user subscribed to failed commands.
        /// </summary>

        public bool SubscribedFailed;

        /// <summary>
        /// Holds a flag indicating whether the current user subscribed to succeeded commands.
        /// </summary>

        public bool SubscribedSucceeded;

        /// <summary>
        /// Holds a flag indicating whether the current user subscribed to triggered commands.
        /// </summary>

        public bool SubscribedTriggered;
    }
}