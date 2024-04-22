using System;
using System.Configuration;
using System.Web.Services;
using System.Linq;
using System.IO;
using CrashReport.Models;


/// <summary>
/// Summary description for RegisterReport
/// </summary>
[WebService(Namespace = "http://tempuri.org/")]
[WebServiceBinding(ConformsTo = WsiProfiles.BasicProfile1_1)]
public class RegisterReport : System.Web.Services.WebService 
{
    public RegisterReport ()
	{
    }

    private DateTime ConvertDumpTimeToDateTime(string timeString)
    {
        //"2006.10.11-13.50.53"

        DateTime newDateTime;

        try
        {
            int yearEndIndex = timeString.IndexOf(".");
            int monthEndIndex = timeString.IndexOf(".", yearEndIndex + 1);
            int dayEndIndex = timeString.IndexOf("-", monthEndIndex + 1);
            int hourEndIndex = timeString.IndexOf(".", dayEndIndex + 1);
            int minuteEndIndex = timeString.IndexOf(".", hourEndIndex + 1);

            string year = timeString.Substring(0, yearEndIndex);
            string month = timeString.Substring(yearEndIndex + 1, monthEndIndex - yearEndIndex - 1);
            string day = timeString.Substring(monthEndIndex + 1, dayEndIndex - monthEndIndex - 1);
            string hour = timeString.Substring(dayEndIndex + 1, hourEndIndex - dayEndIndex - 1);
            string minute = timeString.Substring(hourEndIndex + 1, minuteEndIndex - hourEndIndex - 1);
            string second = timeString.Substring(minuteEndIndex + 1);

            newDateTime = new DateTime(int.Parse(year), int.Parse(month), int.Parse(day), int.Parse(hour), int.Parse(minute), int.Parse(second));
        }
        catch (Exception)
        {
            newDateTime = new DateTime();
        }
        return newDateTime;
    }

    /**
     * CreateNewCrash - creates a new record in the appropriate table from the parameters.
     * 
     * @return int - the unique identifier of the new record.  This will be -1 on failure. 
     */

    [WebMethod]
    public int CreateNewCrash(int AutoReporterId, string ComputerName, string UserName, string GameName, string PlatformName, string LanguageExt,
        string TimeOfCrash, string BuildVer, string ChangelistVer, string CommandLine, string BaseDir,
        string CallStack, string EngineMode)
    {
        string LogFileName = ConfigurationManager.AppSettings["LogFileName"];
        int newID = -1;

        StreamWriter LogFile = null;
        try
        {
            LogFile = new StreamWriter(LogFileName, true); 
        }
        catch (Exception)
        {
            // Ignore cases where the log file can't be opened (another instance may be writing to it already)
        }

        DateTime currentDate = DateTime.Now;
        if (LogFile != null)
        {
            LogFile.WriteLine("");
            LogFile.WriteLine("Creating new report..." + currentDate.ToString("G"));
        }


        try
        {
            BuggRepository mBuggRepository = new BuggRepository();
            //TODO Catch exception if we fail to connect to the database
        
            //Check User First
            User User = mBuggRepository.AddUser(UserName);
                
            Crash NewCrash = new Crash();
            NewCrash.AutoReporterID = AutoReporterId;
            NewCrash.BaseDir = BaseDir;
            NewCrash.BuildVersion = BuildVer;
            NewCrash.ChangeListVersion = ChangelistVer;
            NewCrash.CommandLine = CommandLine;
            NewCrash.ComputerName = ComputerName;
            NewCrash.EngineMode = EngineMode;
            NewCrash.GameName = GameName;
            NewCrash.LanguageExt = LanguageExt;
            NewCrash.PlatformName = PlatformName;
            NewCrash.RawCallStack = CallStack;
            NewCrash.TimeOfCrash = ConvertDumpTimeToDateTime(TimeOfCrash);
            NewCrash.UserName =UserName;
            NewCrash.Status = "New";

            mBuggRepository.Insert(NewCrash);
            mBuggRepository.SubmitChanges();

            newID = NewCrash.Id;
            mBuggRepository.BuildPattern(NewCrash);
            mBuggRepository.AddCrashToBugg(NewCrash);
                              
        }
        catch (Exception e)
        {

            if (LogFile != null)
            {

                LogFile.WriteLine("Exception caught!");
                LogFile.WriteLine(e.Message);
                LogFile.Close();
            }

            return newID;
        }
        //TODO Catch any execptions from failing to open the connection to the db.
       /*     catch (conn e)
            {
                if (LogFile != null)
                {
                    LogFile.WriteLine("Failed to open connection to database!");
                    LogFile.WriteLine("ConnectionString");
                    LogFile.WriteLine(e.Message);
                    LogFile.Close();
                }
                return -1;
            }*/


        if (LogFile != null)
        {
            LogFile.WriteLine("Successfully created new record with id " + newID.ToString());
            LogFile.Close();
        }


        return newID;
    }


       
    /**
     * AddCrashDescription - updates the record identified by rowID with a crash description
     * 
     * @param rowID - the id of the row to update
     * @param CrashDescription - the new crash description
     * 
     * @return bool - true if successful
     */
    [WebMethod]
    public bool AddCrashDescription(int rowID, string CrashDescription, string Summary)
    {
            string LogFileName = ConfigurationManager.AppSettings["LogFileName"];

			StreamWriter LogFile = null;
			try
			{
				LogFile = new StreamWriter( LogFileName, true );
			}
			catch( Exception )
			{
				// Ignore cases where the log file can't be opened (another instance may be writing to it already)
			}

            int rowsAffected = 0;

            try
            {

               CrashReportDataContext mCrashDataContext = new CrashReportDataContext();
               Crash CrashToUpdate = mCrashDataContext.Crashes.Where(c => c.Id == rowID).First();
               CrashToUpdate.Description = CrashDescription;
               CrashToUpdate.Summary = Summary;
               mCrashDataContext.SubmitChanges();

            }
            catch (Exception e)
            {
                if (LogFile != null)
                {
                    LogFile.WriteLine("Exception caught!");
                    LogFile.WriteLine(e.Message);
                    LogFile.Close();
                }
                return false;
            }

            if (LogFile != null)
            {
                LogFile.WriteLine("Successfully updated Crash Description, " + rowsAffected.ToString() + " Rows updated");
                LogFile.Close();
            }

        return true;
    }
}

