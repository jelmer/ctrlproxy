// ctrlproxy.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "ctrlproxyDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define SYSTRAY_QUICK_ID WM_USER+1

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyApp

BEGIN_MESSAGE_MAP(CCtrlproxyApp, CWinApp)
	//{{AFX_MSG_MAP(CCtrlproxyApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyApp construction

CCtrlproxyApp::CCtrlproxyApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CCtrlproxyApp object

CCtrlproxyApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyApp initialization

BOOL CCtrlproxyApp::InitInstance()
{
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	
	dlg = new CCtrlproxyDlg();
	m_pMainWnd = dlg;
	dlg->Create(CCtrlproxyDlg::IDD);
	dlg->ShowWindow(SW_HIDE);

	NOTIFYICONDATA dat;
	dat.cbSize = sizeof(NOTIFYICONDATA);
	dat.hWnd = dlg->m_hWnd;
	dat.uFlags = NIF_ICON + NIF_TIP + NIF_MESSAGE;
	dat.hIcon = LoadIcon(IDR_MAINFRAME);
	dat.uCallbackMessage = CTRLPROXY_TRAY_ICON;
	strcpy(dat.szTip, "CtrlProxy manager");
	Shell_NotifyIcon(NIM_ADD, &dat);

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return TRUE;
}


