//=============================================================================
//	ToolDropdownRadioButton.cs: A radio button that has two images
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace CustomControls
{
    public class ToolDropdownItem
    {
        public ToolDropdownItem(BitmapImage inuncheckedimage, BitmapImage incheckedimage, BitmapImage indisabledimage, String intooltip)
        {
            uncheckedimage = inuncheckedimage;
            checkedimage = incheckedimage;
			disabledimage = indisabledimage;
            tooltip = intooltip;
        }

        public BitmapImage UncheckedImage
        {
            get { return uncheckedimage; }
            set { uncheckedimage = value; }
        }

        public BitmapImage DisabledImage
        {
            get { return disabledimage; }
            set { disabledimage = value; }
        }

        public BitmapImage CheckedImage
        {
            get { return checkedimage; }
            set { checkedimage = value; }
        }

        public String ToolTip
        {
            get { return tooltip; }
            set { tooltip = value; }
        }

        private BitmapImage uncheckedimage;
        private BitmapImage checkedimage;
        private BitmapImage disabledimage;
        private String tooltip;
    }

    public class ToolDropdownItems : ObservableCollection<ToolDropdownItem>
    {
        public ToolDropdownItems()
        : base()
        {
        }
    }

    public class ToolDropdownRadioButton : System.Windows.Controls.RadioButton, INotifyPropertyChanged
	{
        public event PropertyChangedEventHandler PropertyChanged;
    
        static ToolDropdownRadioButton()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( ToolDropdownRadioButton ), new FrameworkPropertyMetadata( typeof( ToolDropdownRadioButton ) ) );
        }

        public ToolDropdownRadioButton()
        {
            ListItems = new ToolDropdownItems();
        }
       
		/// Called when a new template is applied to this control.
        public override void OnApplyTemplate()
        {
            base.OnApplyTemplate();

            m_Popup = (Popup)Template.FindName("Popup", this);
            m_ListBox = (ListBox)Template.FindName("DropdownList", this);

            m_ListBox.SelectionChanged += new SelectionChangedEventHandler(m_ListBox_SelectionChanged);
            this.Click += new RoutedEventHandler(Button_Click);
            AddHandler(MouseLeftButtonDownEvent, new MouseButtonEventHandler(Button_MouseDown), true);
        }
        
		public BitmapImage UncheckedImage
		{
			get { return (BitmapImage)GetValue( UncheckedImageProperty ); }
			set { SetValue( UncheckedImageProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for UncheckedImage.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty UncheckedImageProperty =
			DependencyProperty.Register( "UncheckedImage", typeof( BitmapImage ), typeof( ToolDropdownRadioButton ), new UIPropertyMetadata( null ) );

        public BitmapImage DisabledImage
        {
            get { return (BitmapImage)GetValue(DisabledImageProperty); }
            set { SetValue(DisabledImageProperty, value); }
        }

        // Using a DependencyProperty as the backing store for DisabledImage.  This enables animation, styling, binding, etc...
        public static readonly DependencyProperty DisabledImageProperty =
            DependencyProperty.Register("DisabledImage", typeof(BitmapImage), typeof(ToolDropdownRadioButton), new UIPropertyMetadata(null));


		public BitmapImage CheckedImage
		{
			get { return (BitmapImage)GetValue( CheckedImageProperty ); }
			set { SetValue( CheckedImageProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for CheckedImage.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty CheckedImageProperty =
			DependencyProperty.Register( "CheckedImage", typeof( BitmapImage ), typeof( ToolDropdownRadioButton ), new UIPropertyMetadata( null ) );

        public ToolDropdownItems ListItems
        {
            get { return (ToolDropdownItems)GetValue(ListItemsProperty); }
            set { SetValue(ListItemsProperty, value); }
        }

        public int SelectedIndex 
        {
            get { return selectedindex; }
            set {
                selectedindex = value;
                OnPropertyChanged("SelectedIndex");
            }
        }

        // Using a DependencyProperty as the backing store for ListItems.
        public static readonly DependencyProperty ListItemsProperty =
            DependencyProperty.Register("ListItems", typeof(ToolDropdownItems), typeof(ToolDropdownRadioButton), new UIPropertyMetadata(null));

        public static readonly RoutedEvent ToolSelectionChangedEvent = EventManager.RegisterRoutedEvent(
            "ToolSelectionChanged",
            RoutingStrategy.Bubble,
            typeof(RoutedEventHandler),
            typeof(ToolDropdownRadioButton)
            );

        public event RoutedEventHandler ToolSelectionChanged
        {
            add { AddHandler(ToolSelectionChangedEvent, value); }
            remove { RemoveHandler(ToolSelectionChangedEvent, value); }
        }

        private void RaiseToolSelectionChanged()
        {
            RoutedEventArgs args = new RoutedEventArgs();
            args.RoutedEvent = ToolDropdownRadioButton.ToolSelectionChangedEvent;
            args.Source = this;
            RaiseEvent(args);
        }

        void Button_Click(object sender, RoutedEventArgs e)
        {
            if (bWasChecked)
            {
                m_ListBox.SelectedItem = null;
                m_Popup.IsOpen = true;
                bWasChecked = false;
            }
            else
            {
                RaiseToolSelectionChanged();
            }
        }

        void Button_MouseDown(object sender, MouseButtonEventArgs e)
        {
            bWasChecked = this.IsChecked == true;
        }

        void m_ListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (m_ListBox.SelectedIndex >= 0 && m_ListBox.SelectedIndex < ListItems.Count)
            {
                CheckedImage = ListItems[m_ListBox.SelectedIndex].CheckedImage;
                UncheckedImage = ListItems[m_ListBox.SelectedIndex].UncheckedImage;
                DisabledImage = ListItems[m_ListBox.SelectedIndex].DisabledImage;
                SelectedIndex = m_ListBox.SelectedIndex;
                RaiseToolSelectionChanged();
            }
            m_Popup.IsOpen = false;
        }

        // Create the OnPropertyChanged method to raise the event
        protected void OnPropertyChanged(string name)
        {
            PropertyChangedEventHandler handler = PropertyChanged;
            if (handler != null)
            {
                handler(this, new PropertyChangedEventArgs(name));
            }
        }

        int selectedindex = 0;
        bool bWasChecked = false;
        Popup m_Popup = null;
        ListBox m_ListBox = null;
	}
}
