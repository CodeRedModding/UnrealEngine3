// 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Xml;
using System.Xml.Linq;


namespace MobileShaderAnalyzer

/// <summary>
/// The MobileShaderAnalyzer is a simple tool used to determine what materials and mobile shader keys are
/// being cooked and used for an application.
/// To use, first cook with -DebugFullMaterialReport to generate source XML for this tool
/// You can also run the application with -DebugShowWarmedKeys to create warm data (outputs to debug window/Unreal Console, paste into text file for import)
/// </summary>
{
    public partial class Form1 : Form
    {
        /// <summary>
        /// Describes each setting of a shader key that differs from the default values
        /// The Value field will be used for settings that are not binary in nature
        /// </summary>
        struct SettingDescription
        {
            public string Name;
            public int Value;
        }

        /// <summary>
        /// Describes a mobile shader key, and lists all materials that generated it
        /// </summary>
        class KeyDescription
        {
            public string CodeString;
            public List<SettingDescription> Settings;
            public List<string> MaterialNames;
        }

        /// <summary>
        /// Describes a material, and lists all mobile shader keys that it creates
        /// </summary>
        class MaterialDescription
        {
            public string Name;
            public List<string> KeyCodeStrings;
        }

        /// <summary>
        /// Describes the display mode of the main data grid (showing materials or keys)
        /// </summary>
        enum DisplayMode
        {
            DisplayMode_Mats,
            DisplayMode_Keys
        }

        /// <summary>
        /// All keys loaded from the source XML
        /// </summary>
        private List<KeyDescription> mAllKeys;
        /// <summary>
        /// All materials loaded from the source XML
        /// </summary>
        private List<MaterialDescription> mAllMaterials;
        /// <summary>
        /// All keys that were warmed (must generate and load warming data to see)
        /// </summary>
        private List<KeyDescription> mWarmedKeys;
        /// <summary>
        /// The source XML document
        /// </summary>
        private XDocument mRootDoc;
        /// <summary>
        /// Current display mode of the main data grid
        /// </summary>
        private DisplayMode mCurDisplayMode;
        /// <summary>
        /// Holds all sourcekeys used for the current main data grid (unfiltered)
        /// </summary>
        private List<KeyDescription> mCurSourceKeys;
        /// <summary>
        /// Holds all source materials used for the current main data grid (unfiltered)
        /// </summary>
        private List<MaterialDescription> mCurSourceMaterials;
        /// <summary>
        /// Are we filtering the main display
        /// </summary>
        private bool mIsFiltering = false;
        /// <summary>
        /// Are we filtering based on warming data
        /// </summary>
        private bool mIsWarmFitering = false;
        /// <summary>
        /// Prefix to search imported warm data for.  must be in sync with prefix used by the application
        /// </summary>
        private const string mWarmPrefix = "[WARM KEY]:";
        /// <summary>
        /// Our popup form displaying keys similar to the current selection when the user requests it
        /// </summary>
        private FormCompareItem mCompareItemForm;
        /// <summary>
        /// Holds a material that we'd like to compare the currently selected material against
        /// </summary>
        private MaterialDescription mCompareMaterial;

        public Form1()
        {
            InitializeComponent();
            ResetLists();
            ComboBoxReport.SelectedIndex = 0;
        }


        /// <summary>
        /// Load main XML data
        /// </summary>
        /// <param name="Filename"></param>
        private void LoadXMLSource(string Filename)
        {
            if (!File.Exists(Filename))
            {
                LabelSourceXML.Text = "None loaded";
                return;
            }

            // try to load our potentially massive XML
            ResetLists();
            ResetUI();

            mRootDoc = XDocument.Load(Filename);
            LabelSourceXML.Text = Path.GetFileName(Filename);

            // fill our key and material arrays
            // Keys
            var Keys = (from val in mRootDoc.Element("ROOT").Element("KEYS").Descendants("KEY") 
                        select new { 
                            CodeString = val.Attribute("code"),
                            SettingList = val.Descendants("SETTING"),
                            MaterialList = val.Descendants("MATERIALS").Descendants("MATERIAL")
                        });

            foreach (var elem in Keys)
            {
                KeyDescription KeyDesc = new KeyDescription();
                KeyDesc.MaterialNames = new List<string>();
                KeyDesc.Settings = new List<SettingDescription>();

                KeyDesc.CodeString = elem.CodeString.ToString();
                foreach (var settingelem in elem.SettingList)
                {
                    string Name = settingelem.Attribute("name").ToString();
                    int Val = 0;
                    object ValueObject = settingelem.Attribute("value");
                    if (ValueObject != null)
                    {
                        XAttribute ValueAttribute = settingelem.Attribute("value") as XAttribute;
                        string ValueString = ValueAttribute.Value.ToString();
                        Int32.TryParse(ValueString, out Val);
                    }
                    SettingDescription SettingDesc;
                    SettingDesc.Name = Name;
                    SettingDesc.Value = Val;
                    KeyDesc.Settings.Add(SettingDesc);
                }
                foreach (var materialselem in elem.MaterialList)
                {
                    string MaterialName = materialselem.Attribute("name").ToString();
                    KeyDesc.MaterialNames.Add(MaterialName);
                }
                mAllKeys.Add(KeyDesc);
            }

            // Materials
            var Materials = (from val in mRootDoc.Element("ROOT").Element("MATERIALS").Descendants("MATERIAL")
                        select new
                        {
                            Name = val.Attribute("name"),
                            Keys = val.Descendants("KEYS").Descendants("KEY"),
                        });

            foreach (var elem in Materials)
            {
                MaterialDescription MatDesc = new MaterialDescription();
                MatDesc.KeyCodeStrings = new List<string>();

                MatDesc.Name = elem.Name.ToString();
                foreach (var curkey in elem.Keys)
                {
                    string code = curkey.Attribute("code").ToString();
                    MatDesc.KeyCodeStrings.Add(code);
                }
                mAllMaterials.Add(MatDesc);
            }

            PopulateListKeys(DataGridMain, mAllKeys, false);
            StatusLabel2.Text = "#TotalKeys = " + mAllKeys.Count + "  #TotalMats = " + mAllMaterials.Count;
            StatusLabel.Text = "DisplayedItems:" + mAllKeys.Count;
        }

        /// <summary>
        /// Report type has changed, reflect changes in all ui/datagrids
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ComboBoxReport_SelectedIndexChanged(object sender, EventArgs e)
        {
            NumericBoxLimit.Enabled = false;

            // do nothing if no data loaded
            if (mAllKeys.Count == 0)
            {
                return;
            }

            // all keys
            if (ComboBoxReport.SelectedIndex == 0)
            {
                PopulateListKeys(DataGridMain, mAllKeys, false);
            }
            // all materials
            if (ComboBoxReport.SelectedIndex == 1)
            {
                PopulateListMats(DataGridMain, mAllMaterials, false);
            }
            // keys from few materials
            if (ComboBoxReport.SelectedIndex == 2)
            {
                NumericBoxLimit.Enabled = true;
                // get threshold from spinner
                int threshold = (int)(NumericBoxLimit.Value);
                List<KeyDescription> results = mAllKeys.FindAll(delegate(KeyDescription key)
                {
                    return (key.MaterialNames.Count <= threshold);
                });
                PopulateListKeys(DataGridMain, results, false);
            }

            // materials creating few keys
            if (ComboBoxReport.SelectedIndex == 3)
            {
                NumericBoxLimit.Enabled = true;
                // get threshold from spinner
                int Threshold = (int)(NumericBoxLimit.Value);
                List<MaterialDescription> Results = mAllMaterials.FindAll(delegate(MaterialDescription mat)
                {
                    // iterate key codes in materials, and see how many materials created them, if
                    // below threshold, add this material
                    foreach (string keycode in mat.KeyCodeStrings)
                    {
                        KeyDescription KeyDesc = GetKeyDesc(mAllKeys, keycode);
                        if (KeyDesc.MaterialNames.Count <= Threshold)
                        {
                            return true;
                        }
                    }
                    return false;
                });
                PopulateListMats(DataGridMain, Results, false);
            }
        }

        /// <summary>
        /// Populate a datagrid with a list of shader keys
        /// </summary>
        /// <param name="viewToChange"></param>
        /// <param name="keylist"></param>
        /// <param name="FilterChangeOnly">If this is just a filtering change, we don't change the source data</param>
        private void PopulateListKeys(DataGridView ViewToChange, List<KeyDescription> KeyList, bool FilterChangeOnly)
        {
            bool IsFiltering = mIsFiltering || mIsWarmFitering;
            if (ViewToChange.Equals(DataGridMain))
            {
                ResetUI();
                mCurDisplayMode = DisplayMode.DisplayMode_Keys;
                if (!FilterChangeOnly)
                {
                    // set source data before filtering
                    mCurSourceKeys = KeyList;
                }
                else
                {
                    // just a filter change, use our saved source data
                    KeyList = mCurSourceKeys;
                }
            }
            else
            {
                // dont filter non main grids
                IsFiltering = false;
            }


            // datagrid
            DataTable NewDataTable = new DataTable("KeyList");
            DataColumn NewDataCol;
            DataRow NewDataRow;
            DataSet NewDataSet = new DataSet();

            NewDataCol = new DataColumn();
            NewDataCol.ColumnName = "Code";
            NewDataCol.DataType = typeof(string);
            NewDataTable.Columns.Add(NewDataCol);

            NewDataCol = new DataColumn();
            NewDataCol.ColumnName = "NumMats";
            NewDataCol.DataType = typeof(int);
            NewDataTable.Columns.Add(NewDataCol);

            NewDataCol = new DataColumn();
            NewDataCol.ColumnName = "FirstMat";
            NewDataCol.DataType = typeof(string);
            NewDataTable.Columns.Add(NewDataCol);

            NewDataSet.Tables.Add(NewDataTable);

            // track warmed items for display
            List<int> WarmedIndices = new List<int>();

            // fill table(s)
            foreach (KeyDescription key in KeyList)
            {
                // default to adding items if their filter is off
                bool AddFilter = !mIsFiltering;
                bool AddWarm = !mIsWarmFitering;

                if (!IsFiltering)
                {
                    AddFilter = true;
                    AddWarm = true;
                }
                else
                {
                    if (mIsFiltering)
                    {
                        string FilterText = TextBoxFilter.Text.ToLower();
                        // allow multiple filters separated by commas
                        char[] SplitChars = {','};
                        string[] AllFilters = FilterText.Split(SplitChars);
                        bool BackupAddFilter = AddFilter;
                        foreach (string CurFilter in AllFilters)
                        {
                            AddFilter = BackupAddFilter;
                            bool SearchCondition = true;
                            string LocalFilter = CurFilter;
                            // if search text begins with !, invert filter
                            if (LocalFilter[0] == '!')
                            {
                                SearchCondition = false;
                                LocalFilter = LocalFilter.Substring(1);
                                // default to adding item, then remove if doesn't pass the filter
                                AddFilter = true;
                            }
                            // look for Codes or matnames with the selected filter
                            if (key.CodeString.ToLower().Contains(LocalFilter))
                            {
                                AddFilter = SearchCondition;
                            }
                            foreach (string MatName in key.MaterialNames)
                            {
                                if (MatName.ToLower().Contains(LocalFilter))
                                {
                                    AddFilter = SearchCondition;
                                    break;
                                }
                            }
                            foreach (SettingDescription Setting in key.Settings)
                            {
                                if (Setting.Name.ToLower().Contains(LocalFilter))
                                {
                                    AddFilter = SearchCondition;
                                    break;
                                }
                            }

                            // break out if we've filtered this item out, otherwise keep going
                            if (AddFilter == false)
                            {
                                break;
                            }

                        }  //end for all filters
                    }

                    if (mIsWarmFitering)
                    {
                        // see if key is in warm list
                        bool IsWarmed = (GetKeyDesc(mWarmedKeys, key.CodeString) != null);
                        // are we including or excluding warmed
                        if (IsWarmed && RadioButtonWarmOnly.Checked)
                        {
                            AddWarm = true;
                        }

                        if (!IsWarmed && RadioButtonUnWarmedOnly.Checked)
                        {
                            AddWarm = true;
                        }
                    }
                }
                if (AddFilter && AddWarm)
                {
                    NewDataRow = NewDataTable.NewRow();
                    NewDataRow["Code"] = key.CodeString;
                    NewDataRow["NumMats"] = key.MaterialNames.Count;
                    NewDataRow["FirstMat"] = key.MaterialNames[0];
                    NewDataTable.Rows.Add(NewDataRow);
                    // bold if warmed
                    if (GetKeyDesc(mWarmedKeys, key.CodeString) != null)
                    {
                        WarmedIndices.Add(NewDataTable.Rows.Count - 1);
                    }
                }
            }

            ViewToChange.DataSource = NewDataSet;
            ViewToChange.DataMember = "KeyList";
            ViewToChange.AutoResizeColumn(0);
            StatusLabel.Text = "DisplayedItems:" + NewDataTable.Rows.Count;

            // set warmed items
            foreach (int index in WarmedIndices)
            {
                DataGridViewCellStyle ViewStyle = new DataGridViewCellStyle();
                ViewStyle.ForeColor = Color.White;
                ViewStyle.BackColor = Color.Green;
                ViewToChange.Rows[index].DefaultCellStyle = ViewStyle;
            }


        }

        
        /// <summary>
        /// Populate a datagrid with a list of materials
        /// </summary>
        /// <param name="viewToChange"></param>
        /// <param name="matlist"></param>
        /// <param name="FilterChangeOnly">If this is only a filtering change, don't change the source data for the list</param>
        private void PopulateListMats(DataGridView ViewToChange, List<MaterialDescription> MatList, bool FilterChangeOnly)
        {
            bool IsFiltering = mIsFiltering;
            if (ViewToChange.Equals(DataGridMain))
            {
                ResetUI();
                mCurDisplayMode = DisplayMode.DisplayMode_Mats;
                if (!FilterChangeOnly)
                {
                    // set source data before filtering
                    mCurSourceMaterials = MatList;
                }
                else
                {
                    MatList = mCurSourceMaterials;
                }
            }
            else
            {
                // dont filter non-main
                IsFiltering = false;
            }

            // datagrid
            DataTable NewDataTable = new DataTable("MatList");
            DataColumn NewDataCol;
            DataRow NewDataRow;
            DataSet NewDataSet = new DataSet();

            NewDataCol = new DataColumn();
            NewDataCol.ColumnName = "Name";
            NewDataCol.DataType = typeof(string);
            NewDataTable.Columns.Add(NewDataCol);

            NewDataCol = new DataColumn();
            NewDataCol.ColumnName = "NumKeys";
            NewDataCol.DataType = typeof(int);
            NewDataTable.Columns.Add(NewDataCol);

            NewDataSet.Tables.Add(NewDataTable);

            // fill table(s)
            foreach (MaterialDescription mat in MatList)
            {
                bool Add = false;
                if (!IsFiltering)
                {
                    Add = true;
                }
                else
                {
                    string FilterText = TextBoxFilter.Text.ToLower();
                    // allow multiple filters separated by commas
                    char[] SplitChars = {','};
                    string[] AllFilters = FilterText.Split(SplitChars);
                    bool BackupAdd = Add;
                    foreach (string CurFilter in AllFilters)
                    {
                        Add = BackupAdd;
                        bool SearchCondition = true;
                        string LocalFilter = CurFilter;
                        // if search text begins with !, invert filter
                        if (LocalFilter[0] == '!')
                        {
                            SearchCondition = false;
                            LocalFilter = LocalFilter.Substring(1);
                            Add = true;
                        }
                        // look for Codes or matnames with the selected filter
                        if (mat.Name.ToLower().Contains(LocalFilter))
                        {
                            Add = SearchCondition;
                        }
                        foreach (string KeyCode in mat.KeyCodeStrings)
                        {
                            if (KeyCode.ToLower().Contains(LocalFilter))
                            {
                                Add = SearchCondition;
                                break;
                            }

                            // look in settings for each key
                            KeyDescription KeyDesc = GetKeyDesc(mAllKeys, KeyCode);
                            if (KeyDesc != null)
                            {
                                foreach (SettingDescription settingdesc in KeyDesc.Settings)
                                {
                                    if (settingdesc.Name.ToLower().Contains(LocalFilter))
                                    {
                                        Add = SearchCondition;
                                        break;
                                    }
                                }
                            }
                        }

                        // if we failed the filter, bail out now
                        if (Add == false)
                        {
                            break;
                        }

                    } //end for all filters
                }
                if (Add)
                {
                    NewDataRow = NewDataTable.NewRow();
                    NewDataRow["Name"] = mat.Name;
                    NewDataRow["NumKeys"] = mat.KeyCodeStrings.Count;
                    NewDataTable.Rows.Add(NewDataRow);
                }

            }

            ViewToChange.DataSource = NewDataSet;
            ViewToChange.DataMember = "MatList";
            ViewToChange.AutoResizeColumn(0);
            StatusLabel.Text = "DisplayedItems:" + NewDataTable.Rows.Count;

        }

        /// <summary>
        /// User has selected a new item from the main datagrid display
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void DataGridMain_SelectionChanged(object sender, EventArgs e)
        {
            ListBoxDetails.Items.Clear();
            if (DataGridMain.SelectedRows.Count != 1)
            {
                return;
            }

            if (mCurDisplayMode == DisplayMode.DisplayMode_Keys)
            {
                // key view
                // get row for selected cell

                string KeyCode = DataGridMain.SelectedRows[0].Cells["Code"].Value.ToString();
                // find key in master list
                KeyDescription FoundKey = GetKeyDesc(mAllKeys, KeyCode);

                if (FoundKey != null)
                {
                    ListBoxDetails.Items.Add(KeyCode);
                    foreach (SettingDescription setting in FoundKey.Settings)
                    {
                        if (setting.Value != 0)
                        {
                            ListBoxDetails.Items.Add(String.Format("\tFlag: {0} ({1})", setting.Name, setting.Value));
                        }
                        else
                        {
                            ListBoxDetails.Items.Add(String.Format("\tFlag: {0}", setting.Name));
                        }
                    }
                    foreach (string matname in FoundKey.MaterialNames)
                    {
                        ListBoxDetails.Items.Add("Mat: " + matname);
                    }
                }
            }
            else
            {
                // material view
                string MatName = DataGridMain.SelectedRows[0].Cells["Name"].Value.ToString();
                // find material in master list
                MaterialDescription FoundMat = mAllMaterials.Find(delegate(MaterialDescription mat)
                {
                    return mat.Name.Equals(MatName);
                }
                );

                if (FoundMat != null)
                {
                    ListBoxDetails.Items.Add(MatName);
                    foreach (string keycode in FoundMat.KeyCodeStrings)
                    {
                        ListBoxDetails.Items.Add("\tKey: " + keycode);
                    }
                }

            }

        }

        /// <summary>
        /// User has changed the limit box for certain reports
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void NumericBoxLimit_ValueChanged(object sender, EventArgs e)
        {
            ComboBoxReport_SelectedIndexChanged(sender, e);
        }

        /// <summary>
        /// User has selected some item in the secondary information box
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            // see what was clicked in the listbox, if a code or material, show info in the info view
            string SelectedString = ListBoxDetails.SelectedItem.ToString();
            string SearchString = "Mat: ";
            if (SelectedString.Contains(SearchString))
            {
                // a material line was found, show info about that material
                string MaterialName = SelectedString.Substring(SelectedString.IndexOf(SearchString)+SearchString.Length);
                // find material name in master list
                List<MaterialDescription> SingleItemList = new List<MaterialDescription>();
                MaterialDescription FoundDesc = mAllMaterials.Find(delegate(MaterialDescription mat)
                {
                    return mat.Name.Equals(MaterialName);
                });
                if (FoundDesc != null)
                {
                    SingleItemList.Add(FoundDesc);
                    PopulateListMats(DataGridInfo, SingleItemList, false);
                }
            }

            SearchString = "Key: ";
            if (SelectedString.Contains(SearchString))
            {
                // a material line was found, show info about that material
                string KeyCode = SelectedString.Substring(SelectedString.IndexOf(SearchString) + SearchString.Length);
                // find material name in master list
                List<KeyDescription> SingleKeyItemList = new List<KeyDescription>();
                KeyDescription FoundKeyDesc = GetKeyDesc(mAllKeys, KeyCode);
                if (FoundKeyDesc != null)
                {
                    SingleKeyItemList.Add(FoundKeyDesc);
                    PopulateListKeys(DataGridInfo, SingleKeyItemList, false);
                }
            }
        }

        /// <summary>
        /// User has toggled filtering
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonApplyFilter_Click(object sender, EventArgs e)
        {
            mIsFiltering = !mIsFiltering;
            SetFilterUI(mIsFiltering);
            ApplyChangedFilter();
        }

        /// <summary>
        /// User has changed text in the filtering field
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void TextBoxFilter_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                ApplyChangedFilter();
            }
        }

        /// <summary>
        /// Filter has changed, change displays to accomodate
        /// </summary>
        private void ApplyChangedFilter()
        {
            if (mCurDisplayMode == DisplayMode.DisplayMode_Keys)
            {
                PopulateListKeys(DataGridMain, new List<KeyDescription>(), true);
            }
            else
            {
                PopulateListMats(DataGridMain, new List<MaterialDescription>(), true);
            }
        }

        /// <summary>
        /// User is leaving the filter box, check for change
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void TextBoxFilter_Leave(object sender, EventArgs e)
        {
            ApplyChangedFilter();
        }

        /// <summary>
        /// Write out a text file displaying everything shown in the main data grid
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonWriteReport_Click(object sender, EventArgs e)
        {
            SaveFileDialog.Filter = "Text Files|*.txt";
            SaveFileDialog.AddExtension = true;
            SaveFileDialog.DefaultExt = ".txt";
            DialogResult DiagResult = SaveFileDialog.ShowDialog();
            bool IsSimple = CheckBoxSimpleReport.Checked;

            if (DiagResult != DialogResult.OK)
            {
                return;
            }

            StreamWriter SaveStream = new StreamWriter(SaveFileDialog.OpenFile());

            if (SaveStream == null)
            {
                return;
            }

            foreach (DataGridViewRow row in DataGridMain.Rows)
            {
                if (mCurDisplayMode == DisplayMode.DisplayMode_Keys)
                {
                    string Code = row.Cells["Code"].Value.ToString();
                    // find key 
                    KeyDescription FoundKey = GetKeyDesc(mAllKeys, Code);
                    if (FoundKey != null)
                    {
                        if (!IsSimple)
                        {
                            SaveStream.WriteLine("");
                            SaveStream.WriteLine("===============================================");
                        }
                        SaveStream.WriteLine(FoundKey.CodeString);
                        SaveStream.WriteLine("**Settings**");
                        string SettingString = String.Empty;
                        foreach (SettingDescription setting in FoundKey.Settings)
                        {
                            if (setting.Value != 0)
                            {
                                SettingString += String.Format("{0} ({1}) ", setting.Name, setting.Value);
                            }
                            else
                            {
                                SettingString += String.Format("{0} ", setting.Name);
                            }
                        }
                        SaveStream.WriteLine(SettingString);
                        if (!IsSimple)
                        {
                            SaveStream.WriteLine("--Materials--");
                            foreach (string matname in FoundKey.MaterialNames)
                            {
                                SaveStream.WriteLine(matname);
                            }
                        }

                    }
                }
                else
                {
                    string Name = row.Cells["Name"].Value.ToString();
                    MaterialDescription FoundMat = mAllMaterials.Find(delegate(MaterialDescription mat)
                    {
                        return mat.Name.Equals(Name);
                    }
                    );

                    if (FoundMat != null)
                    {
                        if (!IsSimple)
                        {
                            SaveStream.WriteLine("");
                            SaveStream.WriteLine("===============================================");
                        }
                        SaveStream.WriteLine(FoundMat.Name);
                        if (!IsSimple)
                        {
                            SaveStream.WriteLine("--Keys--");
                            foreach (string keycode in FoundMat.KeyCodeStrings)
                            {
                                SaveStream.WriteLine(keycode);
                            }
                        }
                    }
                }
            }

            SaveStream.Close();

        }

        /// <summary>
        /// Load warming data from a text file 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonLoadWarmData_Click(object sender, EventArgs e)
        {
            OpenFileDialog.DefaultExt = "*.txt";
            OpenFileDialog.Filter = "Text Files |*.txt";
            DialogResult Result = OpenFileDialog.ShowDialog();
            if (Result == DialogResult.OK)
            {
                LoadWarmData(OpenFileDialog.FileName);
            }
        }

        /// <summary>
        /// Open the file given and parse the text for warming data
        /// </summary>
        /// <param name="FileName"></param>
        private void LoadWarmData(string FileName)
        {
            mWarmedKeys = new List<KeyDescription>();
            if (!File.Exists(FileName))
            {
                DisableWarmElements();
                return;
            }

            EnableWarmElements();

            // parse the text file, looking for warm elements
            StreamReader FileReader = new StreamReader(FileName);
            while (!FileReader.EndOfStream)
            {
                string Line = FileReader.ReadLine();
                if (Line.Contains(mWarmPrefix))
                {
                    // we found a warm key, parse it out and add to our list
                    string KeyCode = Line.Substring(Line.IndexOf(mWarmPrefix) + mWarmPrefix.Length);
                    // wrap in quotes and prefix used by XML
                    KeyCode = String.Format("code=\"{0}\"", KeyCode);

                    // find an existing key with this code
                    KeyDescription Description = GetKeyDesc(mAllKeys, KeyCode);

                    if (Description != null)
                    {
                        mWarmedKeys.Add(Description);
                    }
                }
            }

            // update UI
            ApplyChangedFilter();
            LabelWarmData.Text = Path.GetFileName(FileName);

        }

        /// <summary>
        /// Enable UI for warmed elements
        /// </summary>
        private void EnableWarmElements()
        {
            RadioButtonNoFilter.Checked = true;
            RadioButtonNoFilter.Enabled = true;
            RadioButtonWarmOnly.Enabled = true;
            RadioButtonUnWarmedOnly.Enabled = true;
        }

        /// <summary>
        /// Disable UI for warmed elements
        /// </summary>
        private void DisableWarmElements()
        {
            RadioButtonNoFilter.Checked = true;
            RadioButtonNoFilter.Enabled = false;
            RadioButtonWarmOnly.Enabled = false;
            RadioButtonUnWarmedOnly.Enabled = false;
        }

        /// <summary>
        /// User has changed the filtering mode for warmed data, go change display accordingly
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void RadioButtonFilter_CheckedChanged(object sender, EventArgs e)
        {
            // set our filtering modes correctly
            if (RadioButtonNoFilter.Checked)
            {
                mIsWarmFitering = false;
            }
            else
            {
                mIsWarmFitering = true;
            }

            ApplyChangedFilter();
        }

        /// <summary>
        /// User wants to find keys that are similar in nature to this one (only 1 setting off in both directions)
        /// Display a window showing as much
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonShowSimilar_Click(object sender, EventArgs e)
        {
            if (mCurDisplayMode != DisplayMode.DisplayMode_Keys)
            {
                return;
            }

            if (DataGridMain.SelectedRows.Count != 1)
            {
                return;
            }

            if (mCompareItemForm != null)
            {
                mCompareItemForm.Close();
            }

            mCompareItemForm = new FormCompareItem();
            
            // find the given key
            string KeyCode = DataGridMain.SelectedRows[0].Cells["Code"].Value.ToString();
            // find key in master list
            KeyDescription FoundKey = GetKeyDesc(mAllKeys, KeyCode);
            if (FoundKey == null)
            {
                return;
            }

            List<KeyDescription> FoundSimilarKeys = FindSimilarKeys(FoundKey, mAllKeys);

            RichTextBox TargetBox = mCompareItemForm.richTextBox1;

            Font CurFont = TargetBox.Font;
            Font BoldFont = new Font(CurFont, FontStyle.Bold);
            TargetBox.SelectionFont = BoldFont;
            TargetBox.AppendText("Similarity report for key:\n");
            TargetBox.SelectionFont = BoldFont;
            TargetBox.AppendText(FoundKey.CodeString + " : (m[0]=" + FoundKey.MaterialNames[0] + ")\n");
            foreach (SettingDescription outersetdesc in FoundKey.Settings)
            {
                TargetBox.SelectionFont = BoldFont;
                TargetBox.AppendText("\t" + outersetdesc.Name + "\n");
            }
            TargetBox.SelectionFont = BoldFont;
            TargetBox.AppendText("\n");

            TargetBox.SelectionFont = CurFont;

            AppendKeyDeltaText(TargetBox, FoundKey, FoundSimilarKeys);

            mCompareItemForm.Show();
        }

        /// <summary>
        /// Append color coded text to a RichTextBox showing the differences between nearly identical keys
        /// </summary>
        /// <param name="TargetBox"></param>
        /// <param name="FoundKey"></param>
        /// <param name="FoundSimilarKeys"></param>
        private void AppendKeyDeltaText(RichTextBox TargetBox, KeyDescription FoundKey, List<KeyDescription> FoundSimilarKeys)
        {
            foreach (KeyDescription finaldesc in FoundSimilarKeys)
            {
                TargetBox.SelectionColor = System.Drawing.Color.Black;
                TargetBox.AppendText(finaldesc.CodeString + " : (m[0]=" + finaldesc.MaterialNames[0] + ")\n");
                foreach (SettingDescription setdesc in finaldesc.Settings)
                {
                    string OutputString = "\t";
                    // see if this setting is in our current key or not
                    if (!IsSettingInKey(FoundKey, setdesc.Name))
                    {
                        // this setting is unique to the found key, display as such
                        OutputString += "(+) ";
                        TargetBox.SelectionColor = System.Drawing.Color.Green;
                    }
                    OutputString += setdesc.Name;
                    TargetBox.AppendText(OutputString + "\n");
                    // reset color in case it changed
                    TargetBox.SelectionColor = System.Drawing.Color.Black;
                }
                // quick look for settings in found key that aren't in this key
                foreach (SettingDescription FoundSetDesc in FoundKey.Settings)
                {
                    if (!IsSettingInKey(finaldesc, FoundSetDesc.Name))
                    {
                        // this setting is in the found key but not ours, display as such
                        string OutputString = "\t(-) " + FoundSetDesc.Name + "\n";
                        TargetBox.SelectionColor = System.Drawing.Color.Red;
                        TargetBox.AppendText(OutputString);
                        TargetBox.SelectionColor = System.Drawing.Color.Black;
                    }
                }
            }
        }

        /// <summary>
        /// Given a target key
        /// </summary>
        /// <param name="SourceKey"></param>
        /// <param name="KeyList"></param>
        /// <returns></returns>
        private List<KeyDescription> FindSimilarKeys(KeyDescription SourceKey, List<KeyDescription> KeyList)
        {
            List<KeyDescription> SimilarKeys = new List<KeyDescription>();

            // loop through all keys, look for those with roughly the same number of settings (within 1),
            // and with roughly the same settings (within 1)
            foreach (KeyDescription keydesc in KeyList)
            {
                if (Math.Abs(keydesc.Settings.Count - SourceKey.Settings.Count) > 1)
                {
                    continue;
                }

                // skip self
                if (keydesc.CodeString.Equals(SourceKey.CodeString))
                {
                    continue;
                }


                int MatchCountA = 0;
                int MatchCountB = 0;

                // count settings that this key shares with our found key
                foreach (SettingDescription setting in keydesc.Settings)
                {
                    if (IsSettingInKey(SourceKey, setting.Name))
                    {
                        MatchCountA++;
                    }
                }

                // count settings that the found key shares with us
                foreach (SettingDescription settingB in SourceKey.Settings)
                {
                    if (IsSettingInKey(keydesc, settingB.Name))
                    {
                        MatchCountB++;
                    }
                }

                if (Math.Abs(MatchCountA - SourceKey.Settings.Count) > 1)
                {
                    continue;
                }
                if (Math.Abs(MatchCountB - SourceKey.Settings.Count) > 1)
                {
                    continue;
                }

                // if we got here, its close enough
                SimilarKeys.Add(keydesc);
            }

            return SimilarKeys;
        }

        /// <summary>
        /// Close the application
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonClose_Click(object sender, EventArgs e)
        {
            Close();
        }

        /// <summary>
        /// Triggers load main cooked data from an XML file
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonLoadDB_Click(object sender, EventArgs e)
        {
            OpenFileDialog.DefaultExt = "*.xml";
            OpenFileDialog.Filter = "XML Files |*.xml";
            DialogResult Result = OpenFileDialog.ShowDialog();
            if (Result == DialogResult.OK)
            {
                LoadXMLSource(OpenFileDialog.FileName);
            }
        }

        /// <summary>
        /// Reset the main display
        /// </summary>
        private void ResetLists()
        {
            mAllKeys = new List<KeyDescription>();
            mAllMaterials = new List<MaterialDescription>();
            mCurDisplayMode = DisplayMode.DisplayMode_Keys;
            mCurSourceKeys = new List<KeyDescription>();
            mCurSourceMaterials = new List<MaterialDescription>();
            mWarmedKeys = new List<KeyDescription>();
            SetFilterUI(false);
        }

        /// <summary>
        /// Set the filtering UI correctly
        /// </summary>
        /// <param name="Set"></param>
        private void SetFilterUI(bool Set)
        {
            if (Set)
            {
                ButtonApplyFilter.Text = "Apply Text Filter(on):";
                TextBoxFilter.BackColor = Color.LightGreen;
                mIsFiltering = true;
            }
            else
            {
                ButtonApplyFilter.Text = "Apply Text Filter(off):";
                TextBoxFilter.BackColor = Color.LightSalmon;
                mIsFiltering = false;
            }
        }
        /// <summary>
        /// Reset datagrids and listbox
        /// </summary>
        private void ResetUI()
        {
            DataGridMain.DataMember = "";
            ListBoxDetails.Items.Clear();
            DataGridInfo.DataMember = "";
        }

        private void Form1_Load(object sender, EventArgs e)
        {
        }

        /// <summary>
        /// Find a key description with the given code string
        /// </summary>
        /// <param name="DescList"></param>
        /// <param name="Code"></param>
        /// <returns></returns>
        private KeyDescription GetKeyDesc(List<KeyDescription> DescList, string Code)
        {
            KeyDescription FoundDesc = DescList.Find(delegate(KeyDescription desc)
            {
                return desc.CodeString.Equals(Code);
            });

            return FoundDesc;
        }

        /// <summary>
        /// Find a material description with the given name
        /// </summary>
        /// <param name="MatList"></param>
        /// <param name="matname"></param>
        /// <returns></returns>
        private MaterialDescription GetMaterialDesc(List<MaterialDescription> MatList, string matname)
        {
            MaterialDescription MatDesc = MatList.Find(delegate(MaterialDescription desc)
            {
                return desc.Name.Equals(matname);
            });

            return MatDesc;
        }

        /// <summary>
        /// Determine if a key has a particular setting
        /// </summary>
        /// <param name="SourceKey"></param>
        /// <param name="SettingToFind"></param>
        /// <returns></returns>
        bool IsSettingInKey(KeyDescription SourceKey, string SettingToFind)
        {
            foreach (SettingDescription settingsdesc in SourceKey.Settings)
            {
                if (settingsdesc.Name.Equals(SettingToFind))
                {
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Set the currently selected material as one to compare against when selecting a different material
        /// and selecting 'compare materials'
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonSetCompareMaterial_Click(object sender, EventArgs e)
        {
            // make sure we are in material mode and have a material selected
            if (mCurDisplayMode != DisplayMode.DisplayMode_Mats)
            {
                return;
            }

            if (DataGridMain.SelectedRows.Count != 1)
            {
                return;
            }

            string MatName = DataGridMain.SelectedRows[0].Cells[0].Value.ToString();
            MaterialDescription founddesc = GetMaterialDesc(mAllMaterials, MatName);

            if (founddesc != null)
            {
                mCompareMaterial = founddesc;
                LabelCurCompareMaterial.Text = MatName;
            }
            else
            {
                LabelCurCompareMaterial.Text = "No Material Selected";
                mCompareMaterial = null;
            }
        }

        /// <summary>
        /// Compare the selected material with the previously selected material (selected with the
        /// 'set material to compare' button)
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ButtonCompareMaterials_Click(object sender, EventArgs e)
        {
            // make sure we are in material mode and have a material selected
            if (mCurDisplayMode != DisplayMode.DisplayMode_Mats)
            {
                return;
            }

            if (DataGridMain.SelectedRows.Count != 1)
            {
                return;
            }

            string MatName = DataGridMain.SelectedRows[0].Cells[0].Value.ToString();
            MaterialDescription founddesc = GetMaterialDesc(mAllMaterials, MatName);

            CompareMaterials(founddesc);

        }

        /// <summary>
        /// Compare the selected material with the previously set 'compare' material
        /// </summary>
        /// <param name="founddesc"></param>
        private void CompareMaterials(MaterialDescription founddesc)
        {
            if ((founddesc == null) || (mCompareMaterial == null) || (founddesc.Equals(mCompareMaterial)))
            {
                return;
            }

            // we have a selected material, check it against the cached material
            List<KeyDescription> KeysInSelectedOnly = new List<KeyDescription>();
            List<KeyDescription> KeysInCompareOnly = new List<KeyDescription>();
            int SimilarKeys = 0;
            // check selected materials keys for dupes in the current compare material
            foreach (string selectedkeystring in founddesc.KeyCodeStrings)
            {
                // see if this key exists in the current materials as well
                string FoundKeyString = mCompareMaterial.KeyCodeStrings.Find(delegate(string StrToCheck)
                {
                    return StrToCheck.Equals(selectedkeystring);
                });
                if (String.IsNullOrEmpty(FoundKeyString))
                {
                    KeyDescription KeyInSelected = GetKeyDesc(mAllKeys, selectedkeystring);
                    if (KeyInSelected != null)
                    {
                        KeysInSelectedOnly.Add(KeyInSelected);
                    }
                }
                else
                {
                    SimilarKeys++;
                }
            }
            // check the other way - for keys in the current compare material that aren't in the selected
            foreach (string comparekeystring in mCompareMaterial.KeyCodeStrings)
            {
                // see if this key exists in the current materials as well
                string FoundKeyString = founddesc.KeyCodeStrings.Find(delegate(string StrToCheck)
                {
                    return StrToCheck.Equals(comparekeystring);
                });
                if (String.IsNullOrEmpty(FoundKeyString))
                {
                    KeyDescription KeyInCompare = GetKeyDesc(mAllKeys, comparekeystring);
                    if (KeyInCompare != null)
                    {
                        KeysInCompareOnly.Add(KeyInCompare);
                    }
                }
            }

            int TotalKeys = Math.Max(founddesc.KeyCodeStrings.Count, mCompareMaterial.KeyCodeStrings.Count);

            // show results
            ShowCompareResults(founddesc, TotalKeys, KeysInSelectedOnly, KeysInCompareOnly, SimilarKeys);
        }

        /// <summary>
        /// Show the results of a material compare in a popup compare window
        /// </summary>
        /// <param name="Mat"></param>
        /// <param name="TotalKeys"></param>
        /// <param name="KeysInSelectedOnly"></param>
        /// <param name="KeysInCompareOnly"></param>
        /// <param name="SimilarKeys"></param>
        private void ShowCompareResults(MaterialDescription Mat, int TotalKeys, List<KeyDescription> KeysInSelectedOnly, List<KeyDescription> KeysInCompareOnly, int SimilarKeys)
        {
            if (mCompareItemForm != null)
            {
                mCompareItemForm.Close();
            }

            mCompareItemForm = new FormCompareItem();

            RichTextBox TargetBox = mCompareItemForm.richTextBox1;
            // fill lists of keydescriptions for our selected and compare materials
            List<KeyDescription> SelectedMaterialKeys = new List<KeyDescription>();
            List<KeyDescription> CompareMaterialKeys = new List<KeyDescription>();
            
            foreach (string selcode in Mat.KeyCodeStrings)
            {
                KeyDescription desc = GetKeyDesc(mAllKeys, selcode);
                SelectedMaterialKeys.Add(desc);
            }

            foreach (string curcode in mCompareMaterial.KeyCodeStrings)
            {
                KeyDescription desc = GetKeyDesc(mAllKeys, curcode);
                CompareMaterialKeys.Add(desc);
            }

            Font CurFont = TargetBox.Font;
            Font BoldFont = new Font(CurFont, FontStyle.Bold);
            TargetBox.SelectionFont = BoldFont;
            TargetBox.AppendText(String.Format("Material report between {0} and {1}\n", Mat.Name, mCompareMaterial.Name));
            TargetBox.SelectionFont = BoldFont;
            if (TotalKeys == SimilarKeys)
            {
                TargetBox.AppendText("EXACT MATCH!\n");
                mCompareItemForm.Show();
                return;
            }

            TargetBox.SelectionFont = CurFont;
            TargetBox.AppendText(String.Format("Similar keys = {0} / {1}\n\n", SimilarKeys, TotalKeys));

            // display all keys found in selected only
            TargetBox.SelectionFont = BoldFont;
            TargetBox.AppendText(String.Format("Similar keys found only in {0}\n", Mat.Name));
            TargetBox.SelectionFont = CurFont;

            foreach (KeyDescription selectedonly in KeysInSelectedOnly)
            {
                // find similar keys in other material, use first one as a comparison
                List<KeyDescription> SimilarKeyList = FindSimilarKeys(selectedonly, CompareMaterialKeys);
                if (SimilarKeyList.Count > 1)
                {
                    SimilarKeyList.RemoveRange(1, SimilarKeyList.Count - 1);
                }
                AppendKeyDeltaText(TargetBox, selectedonly, SimilarKeyList);
            }

            if (KeysInSelectedOnly.Count == 0)
            {
                TargetBox.AppendText("NONE");
            }
            TargetBox.AppendText("\n\n");

            // display all keys found in compare material only
            TargetBox.SelectionFont = BoldFont;
            TargetBox.AppendText(String.Format("Similar keys found only in {0}\n", mCompareMaterial.Name));
            TargetBox.SelectionFont = CurFont;

            foreach (KeyDescription compareonly in KeysInCompareOnly)
            {
                // find similar keys in other material
                List<KeyDescription> SimilarKeyList = FindSimilarKeys(compareonly, SelectedMaterialKeys);
                if (SimilarKeyList.Count > 1)
                {
                    SimilarKeyList.RemoveRange(1, SimilarKeyList.Count - 1);
                }
                AppendKeyDeltaText(TargetBox, compareonly, SimilarKeyList);
            }

            if (KeysInCompareOnly.Count == 0)
            {
                TargetBox.AppendText("NONE");
            }
            TargetBox.AppendText("\n\n");

            mCompareItemForm.Show();
            
        }


    }
}
