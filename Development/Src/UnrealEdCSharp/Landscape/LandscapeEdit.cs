using System.ComponentModel;
using System.Collections.ObjectModel;
using System.Windows.Media.Imaging;
using System.Windows;

namespace WPF_Landscape
{
    // Landscape import layer listbox
    public class LandscapeImportLayer : INotifyPropertyChanged
    {
        private string layerfilename;
        private string layername;
        private bool noblending;
        private float hardness;

        public event PropertyChangedEventHandler PropertyChanged;

        private void NotifyPropertyChanged(string info)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, new PropertyChangedEventArgs(info));
            }
        }

        public LandscapeImportLayer()
        {
            this.noblending = false;
            this.hardness = 0.5f;
        }

        public LandscapeImportLayer(string layerfilename, string layername)
        {
            this.layerfilename = layerfilename;
            this.layername = layername;
            this.noblending = false;
            this.hardness = 0.5f;
        }

        public string LayerName
        {
            get { return layername; }
            set
            {
                if (value != this.layername)
                {
                    this.layername = value;
                    NotifyPropertyChanged("LayerName");
                }
            }
        }

        public string LayerFilename
        {
            get { return layerfilename; }
            set
            {
                if (value != this.layerfilename)
                {
                    this.layerfilename = value;
                    NotifyPropertyChanged("LayerFilename");
                }
            }
        }

        public bool NoBlending
        {
            get { return noblending; }
            set
            {
                if (value != this.noblending)
                {
                    this.noblending = value;
                    NotifyPropertyChanged("NoBlending");
                }
            }
        }

        public float Hardness
        {
            get { return hardness; }
            set
            {
                if (value != this.hardness)
                {
                    this.hardness = value;
                    NotifyPropertyChanged("Hardness");
                }
            }
        }

    }

    public class LandscapeImportLayers : ObservableCollection<LandscapeImportLayer>
    {
        public LandscapeImportLayers()
            : base()
        {
            LandscapeImportLayer L = new LandscapeImportLayer("", "");
            L.PropertyChanged += ItemPropertyChanged;
            Add(L);
        }

        public void CheckNeedNewEntry()
        {
            if (Count == 0 || this[Count - 1].LayerFilename != "" || this[Count - 1].LayerName != "")
            {
                LandscapeImportLayer L = new LandscapeImportLayer("", "");
                L.PropertyChanged += ItemPropertyChanged;
                Add(L);
            }
        }

        void ItemPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            CheckNeedNewEntry();
        }
    }
}
