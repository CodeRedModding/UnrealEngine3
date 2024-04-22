using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;
using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;

namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class PromoteController : BaseController
    {
        #region Actions

        /// <summary>
        /// Retrieves the view for a command's promotable labels.
        /// </summary>
        /// <param name="id">The ID of the promotion command.</param>
        /// <returns>The promotable labels view.</returns>

        public ActionResult Command (int id)
        {
            var model = new PromoteCommandModel();

            model.Details = dataContext.SP_GetCommandDetails(id).SingleOrDefault();
            model.PromotedLabel = dataContext.SP_GetPromotedLabel(id).SingleOrDefault();

            var subscriptions = dataContext.SP_GetCommandSubscriptions(id, GetCurrentUserEmail()).ToList();

            model.SubscribedFailed = (subscriptions.Count(s => s.Type == 'F') > 0);
            model.SubscribedSucceeded = (subscriptions.Count(s => s.Type == 'S') > 0);
            model.SubscribedTriggered = (subscriptions.Count(s => s.Type == 'T') > 0);

            return View(model);
        }

        /// <summary>
        /// Returns the list of promotable labels for the specified command. 
        /// </summary>
        /// <param name="id">The ID of the promotion command.</param>
        /// <returns>Promotable labels view.</returns>

        public ActionResult CommandLabels (int id)
        {
            var model = new PromoteCommandLabelsModel();

            model.CommandID = id;
            model.Labels = dataContext.SP_GetPromotableLabels(id, 20).ToList();
            model.PromotedLabel = dataContext.SP_GetPromotedLabel(id).SingleOrDefault();

            return View(model);
        }

        /// <summary>
        /// Executes the given promotion command with the specified build label.
        /// </summary>
        /// <param name="id">The ID of the promotion command.</param>
        /// <param name="param">The build label.</param>
        /// <returns></returns>

        public ActionResult PromoteLabel (int id, string param)
        {
            dataContext.SP_PromoteBuild(id, param, GetCurrentUserName());

            return null;
        }

        /// <summary>
        /// Returns the build promotion status view.
        /// </summary>
        /// <returns>Promotion status view.</returns>

        public ActionResult Status ()
        {
            var model = new PromoteStatusModel();

            model.PromotionCommands = dataContext.SP_GetPromotionCommands().ToList();

            return View(model);
        }

        #endregion
    }
}
