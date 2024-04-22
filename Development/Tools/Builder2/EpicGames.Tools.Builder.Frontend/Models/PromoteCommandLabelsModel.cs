using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class PromoteCommandLabelsModel
    {
        /// <summary>
        /// Holds the command's ID.
        /// </summary>

        public int CommandID;

        /// <summary>
        /// Holds the collection of promotable labels.
        /// </summary>

        public IEnumerable<SP_GetPromotableLabelsResult> Labels;

        /// <summary>
        /// Holds the currently promoted build label.
        /// </summary>

        public SP_GetPromotedLabelResult PromotedLabel;
    }
}