using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;


namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class CisStatusChangelistsModel
    {
        /// <summary>
        /// Holds the latest CIS changelists.
        /// </summary>

        public IEnumerable<SP_GetCisLatestChangelistsResult> Latest;

        /// <summary>
        /// Holds the collection of pending CIS tasks.
        /// </summary>

        public IEnumerable<SP_GetCisPendingTasksResult> PendingTasks;
    }
}