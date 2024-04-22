using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Web.Mvc;
using EpicGames.Tools.Builder.Frontend.Database;
using EpicGames.Tools.Builder.Frontend.Models;


namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class BuildController : BaseController
    {
        #region Actions

        /// <summary>
        /// Creates the branch details view.
        /// </summary>
        /// <param name="id">Branch name.</param>
        /// <returns>Branch details view.</returns>

        public ActionResult Branch( int id )
        {
            var model = new BuildBranchModel();

            model.Details = dataContext.SP_GetBranchDetails(id).SingleOrDefault();

            return View(model);
        }

        /// <summary>
        /// Returns the list of build commands for the specified branch.
        /// </summary>
        /// <param name="id">Branch identifier.</param>
        /// <returns>Build command list view.</returns>

        public ActionResult BranchCommands( int id )
        {
            return View(dataContext.SP_GetBranchCommands(id).ToList());
        }

        /// <summary>
        /// Creates the change list details view.
        /// </summary>
        /// <param name="id">Changelist identifier.</param>
        /// <returns>Changelist details view.</returns>

        public ActionResult Changelist( int id )
        {
            var model = new BuildChangelistModel();

            model.ChangelistId = id;
            model.Details = dataContext.SP_GetChangelistDetails(id).SingleOrDefault();

            if (model.Details != null)
            {
                model.Details.Submitter = model.Details.Submitter.Replace('_', '.');
                model.BuildLogs = dataContext.SP_GetChangelistBuildLogs(id, model.Details.Branch).ToList();
            }

            model.CisLogs = dataContext.SP_GetChangelistCisLogs(id).ToList();

            return View(model);
        }

        /// <summary>
        /// Creates the build command details view.
        /// </summary>
        /// <param name="id">Command identifier.</param>
        /// <returns>Build command details view.</returns>

        public ActionResult Command( int id )
        {
            var model = new BuildCommandModel();

            model.Branches = dataContext.SP_GetBranchList().ToList();
            model.Details = dataContext.SP_GetCommandDetails(id).SingleOrDefault();
            model.Machines = dataContext.SP_GetMachineStatus("%").ToList();
            
            var subscriptions = dataContext.SP_GetCommandSubscriptions(id, GetCurrentUserEmail()).ToList();

            model.SubscribedFailed = (subscriptions.Count(s => s.Type == 'F') > 0);
            model.SubscribedSucceeded = (subscriptions.Count(s => s.Type == 'S') > 0);
            model.SubscribedTriggered = (subscriptions.Count(s => s.Type == 'T') > 0);

            return View(model);
        }

        /// <summary>
        /// Retrieves the details for the specified build command.
        /// </summary>
        /// <param name="id">ID of the command.</param>
        /// <returns>Command details view.</returns>

        public ActionResult CommandDetails( int id )
        {
            return View(dataContext.SP_GetCommandDetails(id).SingleOrDefault());
        }

        /// <summary>
        /// Retrieves the build log for the specified build command.
        /// </summary>
        /// <param name="id">ID of the command.</param>
        /// <returns>Command log view.</returns>
        
        public ActionResult CommandLog( int id )
        {
            var model = new BuildCommandLogModel();

            model.BuildLogs = dataContext.SP_GetCommandBuildLogs(id, 0, 50).ToList();
            model.Details = dataContext.SP_GetCommandDetails(id).SingleOrDefault();

            return View(model);
        }

        /// <summary>
        /// Displays the desktop performance page.
        /// </summary>
        /// <returns>Desktop performance view.</returns>

        public ActionResult DesktopPerformance( )
        {
            return View();
        }

        /// <summary>
        /// Disables maintenance mode for the specified build machine.
        /// </summary>
        /// <param name="id">Machine name.</param>
        /// <returns></returns>

        public ActionResult DisableMachineMaintenance( string id )
        {
            dataContext.SP_SetMachineMaintenance(id, 0);

            return null;
        }

        /// <summary>
        /// Enables maintenance mode for the specified build machine.
        /// </summary>
        /// <param name="id">Machine name.</param>
        /// <returns></returns>

        public ActionResult EnableMachineMaintenance( string id )
        {
            dataContext.SP_SetMachineMaintenance(id, 1);

            return null;
        }

        /// <summary>
        /// Creates the build failures view.
        /// </summary>
        /// <returns>Build failures view.</returns>

        public ActionResult Failures( )
        {
            var model = new BuildFailuresModel();

            model.FailedCommands = dataContext.SP_GetFailedCommands(999999).ToList();

            return View(model);
        }

        /// <summary>
        /// Creates the infrastructure view.
        /// </summary>
        /// <returns>Infrastructure view.</returns>

        public ActionResult Infrastructure( )
        {
            return View();
        }

		/// <summary>
		/// Creates the infrastructure machine list view.
		/// </summary>
		/// <returns>Infrastructure machine list view</returns>
        public ActionResult InfrastructureList( )
        {
            return View(dataContext.SP_GetMachineStatus("%").ToList());
        }

		/// <summary>
		/// Locks the specified branch.
		/// </summary>
		/// <param name="id">The ID of the branch to lock.</param>
		/// <returns></returns>
		public ActionResult LockBranch(int id)
		{
			dataContext.SP_SetBranchLockDown(id, 1);

			return null;
		}

        /// <summary>
        /// Returns a build log file.
        /// </summary>
        /// <param name="id">The branch for which the log file was generated.</param>
        /// <param name="param">The path to the log file.</param>
        /// <returns></returns>
        public ActionResult LogFile( string id, string param )
        {
            string path = "file://epicgames.net/Root/UE3/Builder/BuilderFailedLogs/" + id + "/" + param;

            try
            {
                using (var webClient = new WebClient())
                {
                    return Content("<pre>" + webClient.DownloadString(path) + "</pre>");
                }
            }
            catch (Exception e)
            {
                return Content("File not found - " + e.Message);
            }
        }

        /// <summary>
        /// Retrieves the view for a particular build machine.
        /// </summary>
        /// <param name="id">Machine name.</param>
        /// <returns>Build machine details view.</returns>

        public ActionResult Machine( string id )
        {
            var model = new BuildMachineModel();

            model.MachineName = id;

            return View(model);
        }

        /// <summary>
        /// Returns the list of build logs for the specified machine.
        /// </summary>
        /// <param name="id">Machine name.</param>
        /// <returns>Build log list view.</returns>

        public ActionResult MachineBuildLogs( string id )
        {
            return View(dataContext.SP_GetMachineBuildLogs(id, 0, 25).ToList());
        }

        /// <summary>
        /// Retrieves the details for the specified build machine.
        /// </summary>
        /// <param name="id">Name of the machine.</param>
        /// <returns>Machine details view.</returns>

        public ActionResult MachineDetails( string id )
        {
            return View(dataContext.SP_GetMachineDetails(id).SingleOrDefault());
        }

        /// <summary>
        /// Creates the build status view.
        /// </summary>
        /// <returns>Build status view.</returns>

        public ActionResult Status( )
        {
            var model = new BuildStatusModel();

            model.Branches = dataContext.SP_GetBuildBranches().ToList();

            return View(model);
        }

        /// <summary>
        /// Creates a status view for all build commands.
        /// </summary>
        /// <returns></returns>

        public ActionResult StatusCommands( )
        {
            return View(dataContext.SP_GetBuildCommands().ToList());
        }

        /// <summary>
        /// Stops the in-progress build with the specified identifier.
        /// </summary>
        /// <param name="id">Build identifier.</param>
        /// <returns></returns>

        public ActionResult Stop( int id )
        {
            dataContext.SP_StopCommand(id, GetCurrentUserName());

            return null;
        }

        /// <summary>
        /// Subscribes the current user to build command notifications.
        /// </summary>
        /// <param name="id">Build command identifier.</param>
        /// <param name="type">The type of subscription (trigger, success, failure).</param>
        /// <returns>Nothing.</returns>

        public ActionResult SubscribeCommand( int id, byte type )
        {
            dataContext.SP_AddCommandSubscription(id, type, GetCurrentUserEmail());

            return null;
        }

        /// <summary>
        /// Triggers a build command.
        /// </summary>
        /// <param name="id">The ID of the command to trigger.</param>
        /// <returns>Nothing.</returns>

        public ActionResult Trigger( int id )
        {
            dataContext.SP_TriggerCommand(id, GetCurrentUserName());

            return null;
        }

        /// <summary>
        /// Displays the UE4 build times chart page.
        /// </summary>
        /// <returns>Build times view.</returns>

        public ActionResult UE4BuildTimes( )
        {
            return View();
        }

		/// <summary>
		/// Locks the specified branch.
		/// </summary>
		/// <param name="id">The ID of the branch to lock.</param>
		/// <returns></returns>
		public ActionResult UnlockBranch( int id )
		{
			dataContext.SP_SetBranchLockDown(id, 0);

			return null;
		}

        /// <summary>
        /// Unsubscribes the current user from build command notifications.
        /// </summary>
        /// <param name="id">Build command identifier.</param>
        /// <param name="type">The type of subscription (trigger, success, failure).</param>
        /// <returns>Nothing.</returns>

        public ActionResult UnsubscribeCommand( int id, byte type )
        {
            dataContext.SP_RemoveCommandSubscription(id, type, GetCurrentUserEmail());

            return null;
        }

        #endregion
    }
}
