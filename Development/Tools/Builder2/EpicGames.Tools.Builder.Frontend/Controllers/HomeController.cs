using System.Linq;
using System.Web.Mvc;

using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;

namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class HomeController : BaseController
    {
        /// <summary>
        /// Displays the dashboard page.
        /// </summary>
        /// <returns>Dashboard page view.</returns>

        public ActionResult Dashboard ()
        {
            return View();
        }

        /// <summary>
        /// Displays the help page.
        /// </summary>
        /// <returns>Help page view.</returns>

        public ActionResult Help ()
        {
            return View();
        }

        public ActionResult UserProfile (string id)
        {
            var model = new HomeUserProfileModel();

            model.UserName = id;
            model.SubmittedChangelists = dataContext.SP_GetUserChangelists(id.Replace(".", "_"), 0, 15).ToList();

            return View(model);
        }

        public ActionResult Settings ()
        {
            return View();
        }
    }
}
