// ctrlproxyDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "ctrlproxyDlg.h"
#include "CPluginsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyDlg dialog

CCtrlproxyDlg::CCtrlproxyDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CCtrlproxyDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CCtrlproxyDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCtrlproxyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCtrlproxyDlg)
	DDX_Control(pDX, IDC_TAB1, m_Tab);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CCtrlproxyDlg, CDialog)
	//{{AFX_MSG_MAP(CCtrlproxyDlg)
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelchangeTab)
	ON_MESSAGE (CTRLPROXY_TRAY_ICON, OnSysTrayIconClick)
	ON_BN_CLICKED(IDC_CLOSE, OnClose)
	ON_WM_SYSCOMMAND()
	ON_COMMAND(IDM_EXIT, OnExit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyDlg message handlers

BOOL CCtrlproxyDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	TC_ITEM TabCtrlItem;
	TabCtrlItem.mask = TCIF_TEXT;
    TabCtrlItem.pszText = "Status";
	m_Tab.InsertItem( TAB_STATUS, &TabCtrlItem );
    TabCtrlItem.pszText = "Networks";
	m_Tab.InsertItem( TAB_NETWORKS, &TabCtrlItem );
    TabCtrlItem.pszText = "Plugins";
	m_Tab.InsertItem( TAB_PLUGINS, &TabCtrlItem );
	TabCtrlItem.pszText = "Log";
	m_Tab.InsertItem( TAB_LOG, &TabCtrlItem );
	TabCtrlItem.pszText = "Configuration";
	m_Tab.InsertItem( TAB_CONFIGURATION, &TabCtrlItem );
	TabCtrlItem.pszText = "About";
	m_Tab.InsertItem( TAB_ABOUT, &TabCtrlItem );
	
	for(int i = 0; i < NUM_TABS; i++) {
		tabs[i] = new CDialog();
	}

	VERIFY(tabs[TAB_STATUS]->Create(CStatusDlg::IDD, this));
    VERIFY(tabs[TAB_NETWORKS]->Create(CNetworksDlg::IDD, this));
	VERIFY(tabs[TAB_PLUGINS]->Create(CPluginsDlg::IDD, this));
	VERIFY(tabs[TAB_LOG]->Create(CLogDlg::IDD, this));
	VERIFY(tabs[TAB_CONFIGURATION]->Create(CConfigurationDlg::IDD, this));
	VERIFY(tabs[TAB_ABOUT]->Create(CAboutDlg::IDD, this));

	for(i = 0; i < NUM_TABS; i++) {
		CRect rect;
		m_Tab.GetClientRect(&rect);
		VERIFY(tabs[i]->SetWindowPos(GetDlgItem(IDC_TAB1), rect.left + 20, rect.top + 35, rect.Width()-20, rect.Height()-35, SWP_SHOWWINDOW));
	}

	UpdateVisibleWindow();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CCtrlproxyDlg::OnDestroy()
{
	WinHelp(0L, HELP_QUIT);
	CDialog::OnDestroy();
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CCtrlproxyDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CCtrlproxyDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}
void CCtrlproxyDlg::OnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult) 
{
   UpdateVisibleWindow();

   *pResult = 0;
}

void CCtrlproxyDlg::UpdateVisibleWindow()
{
   int current = m_Tab.GetCurSel();

   for(int i = 0; i < NUM_TABS; i++) {
	   ::ShowWindow(tabs[i]->m_hWnd, current == i ? SW_SHOW : SW_HIDE);
   }

      if(current == TAB_CONFIGURATION) {
	   CConfigurationDlg *dlg = (CConfigurationDlg *)tabs[TAB_CONFIGURATION];
		dlg->UpdateTree();
   }

}

afx_msg LONG CCtrlproxyDlg::OnSysTrayIconClick (WPARAM wParam, LPARAM lParam)
{
    switch (lParam)
    {
              case WM_LBUTTONDOWN:
					ShowWindow(SW_RESTORE);
					break;
              case WM_RBUTTONDOWN:
                   ShowQuickMenu ();
                   break ;
    }
           
	return 0;
}

void CCtrlproxyDlg::ShowQuickMenu ()
{
   POINT CurPos;
   int mid = 0;

   CMenu qmenu;
   qmenu.LoadMenu(IDR_POPUP);
   CMenu *popup = qmenu.GetSubMenu(mid);
   
   GetCursorPos (&CurPos);

   // Display the menu. This menu is a popup loaded elsewhere.

   popup->TrackPopupMenu (TPM_RIGHTBUTTON | TPM_RIGHTALIGN,
                   CurPos.x,
                   CurPos.y,this);

} 

void CCtrlproxyDlg::OnClose() 
{
	ShowWindow(SW_HIDE);
	
}

void CCtrlproxyDlg::OnExit() 
{
	// TODO: Add your command handler code here
	
}
