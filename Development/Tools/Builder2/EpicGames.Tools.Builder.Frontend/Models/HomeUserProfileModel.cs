using System.Collections.Generic;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Models
{
    public class HomeUserProfileModel
    {
        public IEnumerable<SP_GetUserChangelistsResult> SubmittedChangelists;

        public string UserName;
    }
}