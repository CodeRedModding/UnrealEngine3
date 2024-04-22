using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;


namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class BuildFailuresModel
    {
        /// <summary>
        /// Holds the collection of failed build commands.
        /// </summary>

        public IEnumerable<SP_GetFailedCommandsResult> FailedCommands;
    }
}