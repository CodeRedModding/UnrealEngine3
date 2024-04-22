using System.Linq;
using System.Web.Mvc;
using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;


namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class CisController : BaseController
    {
        #region Actions

        /// <summary>
        /// Creates the branch details view.
        /// </summary>
        /// <param name="id">Branch identifier.</param>
        /// <returns>Branch details view.</returns>

        public ActionResult Branch( int id )
        {
            var model = new CisBranchModel();

			model.Details = dataContext.SP_GetBranchDetails(id).SingleOrDefault();

            return View(model);
        }

        /// <summary>
        /// Returns the list of CIS tasks for the specified branch.
        /// </summary>
        /// <param name="id">Branch identifier.</param>
        /// <returns>CIS tasks list view.</returns>

        public ActionResult BranchTasks( int id )
        {
            return View(dataContext.SP_GetCisBranchChangelists(id, 50).ToList());
        }

        /// <summary>
        /// Shows the CIS status page.
        /// </summary>
        /// <returns>CIS status page view.</returns>

        public ActionResult Status( )
        {
            var model = new CisStatusModel();

            model.Branches = dataContext.SP_GetCisBranches().ToList();

            return View(model);
        }

        /// <summary>
        /// Returns the list of current change lists for all branches.
        /// </summary>
        /// <returns>CIS change lists list view.</returns>

        public ActionResult StatusChangelists( )
        {
            var model = new CisStatusChangelistsModel();

            model.Latest = dataContext.SP_GetCisLatestChangelists().ToList();
            model.PendingTasks = dataContext.SP_GetCisPendingTasks().ToList();

            return View(model);
        }

        #endregion
    }
}