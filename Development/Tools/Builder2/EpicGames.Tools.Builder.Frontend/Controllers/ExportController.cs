using EpicGames.Tools.Builder.Frontend.Database;
using System.Linq;
using System.Web.Mvc;


namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class ExportController : BaseController
    {
        #region Actions

        public ActionResult BuildTimes(int counter, int days)
        {
            SetupCsvHeaders(counter.ToString() + ".csv");
            
            Response.Write("Timestamp,Changelist,BuildTimeMilliseconds,MachineID,Submitter,Description\n");
            
            foreach (var d in base.dataContext.SP_GetPerformanceDataDanV(new int?(counter), new int?(days)).ToList())
            {
                if (d.MachineID == 93) // BUILD-07
                {
                    base.Response.Write(
                        d.TimeStamp.ToString() + "," +
                        d.Changelist.ToString() + "," +
                        d.IntValue.ToString() + "," +
                        d.MachineID.ToString() + "," +
                        d.Submitter + ",\"" +
                        d.Description.Replace('\"', '\'') + "\"\n"
                    );
                }                
            }

            return null;   
        }

        #endregion


        #region Implementation

        protected void SetupCsvHeaders(string filename)
        {
            Response.AddHeader("Content-Disposition", string.Format("attachment; filename={0}", filename));
            Response.ContentType = "text/csv";
        }

        #endregion
    }
}
