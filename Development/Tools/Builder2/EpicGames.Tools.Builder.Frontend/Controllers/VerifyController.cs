using System.Linq;
using System.Web.Mvc;

using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;


namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class VerifyController : BaseController
    {
        #region Actions

        /// <summary>
        /// Creates the branch details view.
        /// </summary>
        /// <param name="id">Branch name.</param>
        /// <returns>Branch details view.</returns>

        public ActionResult Branch (string id)
        {
            var model = new VerifyBranchModel();

            model.BranchName = id;

            return View(model);
        }

        /// <summary>
        /// Returns the list of verification commands for the specified branch.
        /// </summary>
        /// <param name="id">Branch identifier.</param>
        /// <returns>Verification command list view.</returns>

        public ActionResult BranchCommands (string id)
        {
            return View(dataContext.SP_GetVerificationBranchCommands(id).ToList());
        }

        /// <summary>
        /// Creates the verification status view.
        /// </summary>
        /// <returns>Build verification view.</returns>

        public ActionResult Status ()
        {
            var model = new VerifyStatusModel();

            model.Branches = dataContext.SP_GetVerificationBranches().ToList();

            return View(model);
        }

        /// <summary>
        /// Creates a status view for all verification commands.
        /// </summary>
        /// <returns></returns>

        public ActionResult StatusCommands ()
        {
            return View(dataContext.SP_GetVerificationBranchCommands("%").ToList());
        }

        #endregion
    }
}
