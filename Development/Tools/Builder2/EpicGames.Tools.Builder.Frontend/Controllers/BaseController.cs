using System.Web.Mvc;

using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public enum BuildCommandTypes
    {
        Build = 0,
        Cis = 1,
        Verification = 2,
        BuildPromotion = 3,
        SendQAChanges = 4,
        Tools = 5,
        Test = 6,
        QABuildPromotion = 7
    }

    public class BaseController : Controller
    {
        #region Fields

        protected DataClassesDataContext dataContext = new DataClassesDataContext();

        #endregion


        #region Implementation

        /// <summary>
        /// Returns the name of the authenticated user.
        /// </summary>
        /// <returns>User name.</returns>

        protected string GetCurrentUserName ()
        {
            string username = User.Identity.Name;

            int Offset = username.LastIndexOf('\\');

            if (Offset >= 0)
            {
                username = username.Substring(Offset + 1);
            }

            return username;
        }

        /// <summary>
        /// Returns the email address of the authenticated user.
        /// </summary>
        /// <returns>Email address.</returns>

        protected string GetCurrentUserEmail ()
        {
            return GetCurrentUserName() + "@epicgames.com";
        }

        #endregion
    }
}
