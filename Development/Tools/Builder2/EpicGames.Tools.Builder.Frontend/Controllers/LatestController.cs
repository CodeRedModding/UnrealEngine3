using System.Linq;
using System.Web.Mvc;

using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;

namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class LatestController : BaseController
    {
        #region Actions

        /// <summary>
        /// Displays the latest status page.
        /// </summary>
        /// <returns>Status page.</returns>

        public ActionResult Status()
        {
            var model = new LatestStatusModel();

            model.LatestBuilds = dataContext.SP_GetLatestBuilds().ToList();

            return View(model);
        }

        #endregion
    }
}
