using System;
using System.Data;
using System.Configuration;
using System.Collections;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using CrashDataFormattingRoutines;

public partial class SingleReportView : System.Web.UI.Page
{
    public CallStackContainer callStack;
    public string CompleteLogFileName;

    protected void Page_Load(object sender, EventArgs e)
    {
        if (IsPostBack)
        {
            DetailsView1.DataBind();
        }
    }

    private string GetLogIniDownloadURLs(string RowID)
    {
        string VirtualSaveFilesPath = ConfigurationManager.AppSettings["VirtualSaveFilesPath"];
        string LocalSaveFilesPath = ConfigurationManager.AppSettings["SaveFilesPath"];
        string RemovedFolderIDVersion = ConfigurationManager.AppSettings["RemovedFolderIDVersion"];

        string downloadURL = "";

        //old format where files were put in a unique folder
        if (int.Parse(RowID) <= int.Parse(RemovedFolderIDVersion))
        {
            string VirtualCompleteLogFileName = VirtualSaveFilesPath + RowID + "/Launch.log";
            string VirtualCompleteIniFileName = VirtualSaveFilesPath + RowID + "/UE3AutoReportIniDump.txt";
            string VirtualCompleteMiniDumpFileName = VirtualSaveFilesPath + RowID + "/MiniDump.dmp";

            string LocalCompleteLogFileName = LocalSaveFilesPath + RowID + "\\Launch.log";
            string LocalCompleteIniFileName = LocalSaveFilesPath + RowID + "\\UE3AutoReportIniDump.txt";
            string LocalCompleteMiniDumpFileName = LocalSaveFilesPath + RowID + "\\MiniDump.dmp";

            if (System.IO.File.Exists(LocalCompleteLogFileName))
            {
                downloadURL += "<a href =\"http://" + VirtualCompleteLogFileName + "\">Log</a> ";
            }

            if (System.IO.File.Exists(LocalCompleteIniFileName))
            {
                downloadURL += "<a href =\"http://" + VirtualCompleteIniFileName + "\">Inis</a> ";
            }

            if (System.IO.File.Exists(LocalCompleteMiniDumpFileName))
            {
                downloadURL += "<a href =\"http://" + VirtualCompleteMiniDumpFileName + "\">MiniDump</a>";
            }
        }
        //new format, files are prefixed by 'ID_'
        else
        {
            string VirtualCompleteLogFileName = VirtualSaveFilesPath + RowID + "_Launch.log";
            string VirtualCompleteIniFileName = VirtualSaveFilesPath + RowID + "_UE3AutoReportIniDump.txt";
            string VirtualCompleteMiniDumpFileName = VirtualSaveFilesPath + RowID + "_MiniDump.dmp";

            string LocalCompleteLogFileName = LocalSaveFilesPath + RowID + "_Launch.log";
            string LocalCompleteIniFileName = LocalSaveFilesPath + RowID + "_UE3AutoReportIniDump.txt";
            string LocalCompleteMiniDumpFileName = LocalSaveFilesPath + RowID + "_MiniDump.dmp";


            if (System.IO.File.Exists(LocalCompleteLogFileName))
            {
                downloadURL += "<a href =\"http://" + VirtualCompleteLogFileName + "\">Log</a> ";
            }

            if (System.IO.File.Exists(LocalCompleteIniFileName))
            {
                downloadURL += "<a href =\"http://" + VirtualCompleteIniFileName + "\">Inis</a> ";
            }

            if (System.IO.File.Exists(LocalCompleteMiniDumpFileName))
            {
                downloadURL += "<a href =\"http://" + VirtualCompleteMiniDumpFileName + "\">MiniDump</a>";
            }
        }

        return downloadURL;
    }
 
   
    protected void DetailsView1_DataBound(object sender, EventArgs e)
    {
        int CallStackIndex = 17;
        DetailsViewRow CallStackRow = DetailsView1.Rows[CallStackIndex];
        string OriginalCallStack = CallStackRow.Cells[1].Text;

        callStack = new CallStackContainer(OriginalCallStack, 100, true, true);

        if (IsPostBack)
        {
            callStack.DisplayFunctionNames = CheckBoxList1.Items[0].Selected;
            callStack.DisplayFileNames = CheckBoxList1.Items[1].Selected;
            callStack.DisplayFilePathNames = CheckBoxList1.Items[2].Selected;
            callStack.DisplayUnformattedCallStack = CheckBoxList1.Items[3].Selected;
        }

        string FormattedCallStack = callStack.GetFormattedCallStack().Replace("\n", "<br>");
        CallStackRow.Cells[1].Text = FormattedCallStack;

        int RowIDIndex = 0;
        int LogLinkIndex = 1;
        DetailsViewRow LogLinkRow = DetailsView1.Rows[LogLinkIndex];
        DetailsViewRow IDRow = DetailsView1.Rows[RowIDIndex];
        string IDString = IDRow.Cells[1].Text;

        LogLinkRow.Cells[1].Text = GetLogIniDownloadURLs(IDString);

    }

    protected void CheckBoxList1_SelectedIndexChanged(object sender, EventArgs e)
    {
       
    }

}
