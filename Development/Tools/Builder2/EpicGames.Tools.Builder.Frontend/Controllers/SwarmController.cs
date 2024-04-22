using System.Web.Mvc;

using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;


namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class SwarmController : Controller
    {
        /// <summary>
        /// Creates the swarm status view.
        /// </summary>
        /// <returns>Build swarm view.</returns>

        public ActionResult Status ()
        {
            return View();
        }

    }
}
