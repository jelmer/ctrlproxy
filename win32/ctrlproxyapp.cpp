// ctrlproxy.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "ctrlproxyDlg.h"
extern "C" {
#include "internals.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
void register_log_function();
}

GMainLoop *main_loop = NULL;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyApp

BEGIN_MESSAGE_MAP(CCtrlproxyApp, CWinApp)
	//{{AFX_MSG_MAP(CCtrlproxyApp)
	ON_COMMAND(IDM_EXIT, OnExit)
	ON_COMMAND(IDM_ABOUT, OnAbout)
	ON_COMMAND(IDM_STATUS, OnStatus)
	//}}AFX_MSG_MAP
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
	
	xmlMemSetup(g_free, (void *(__cdecl *)(unsigned int))g_malloc, (void *(__cdecl *)(void *, unsigned int))g_realloc, g_strdup);
	
	dlg = new CCtrlproxyDlg();
	m_pMainWnd = dlg;
	dlg->Create(CCtrlproxyDlg::IDD);
	dlg->ShowWindow(SW_HIDE);

	register_log_function();	

	NOTIFYICONDATA dat;
	dat.cbSize = sizeof(NOTIFYICONDATA);
	dat.hWnd = dlg->m_hWnd;
	dat.uFlags = NIF_ICON + NIF_TIP + NIF_MESSAGE;
	dat.hIcon = LoadIcon(IDR_MAINFRAME);
	dat.uCallbackMessage = CTRLPROXY_TRAY_ICON;
	strcpy(dat.szTip, "CtrlProxy manager");
	Shell_NotifyIcon(NIM_ADD, &dat);
//	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MASK, log_handler, NULL);
	add_filter_class("", -1);
	add_filter_class("client", 100);
	add_filter_class("replicate", 50);
	add_filter_class("log", 50);

	g_message(_("Logfile opened"));

	configuration_file = "C:\\ctrlproxyrc.txt"; /*FIXME*/
	readConfig(configuration_file);
	
	init_plugins();
	init_networks();

	initialized_hook_execute();

	main_loop = g_main_loop_new(NULL, FALSE);
	return TRUE;
}

const char *get_my_hostname() { return "localhost"; /* FIXME */ } 
const char *get_modules_path() { return "C:\\Documents and Settings\\Jelmer Vernooij\\Desktop\\ctrlproxy-devel\\win32\\Debug"; /*FIXME*/ }
const char *get_shared_path() { return "C:\\Documents and Settings\\Jelmer Vernooij\\Desktop\\ctrlproxy-devel"; /*FIXME*/} 

const char *ctrlproxy_version() { 
	return "FIXME";
}

void clean_exit()
{
	exit(0);
}	

int CCtrlproxyApp::Run() 
{
	g_main_context_iteration(g_main_loop_get_context(main_loop), FALSE);
	
	return CWinApp::Run();
}

int CCtrlproxyApp::ExitInstance() 
{
	GList *gl = get_network_list();
	while(gl) {
		struct network *n = (struct network *)gl->data;
		gl = gl->next;
		if(n) close_network(n);
	}
	
	return CWinApp::ExitInstance();
}

void CCtrlproxyApp::OnExit() 
{
	clean_exit();
	
}

void CCtrlproxyApp::OnAbout() 
{
	// TODO: Add your command handler code here
	
}

void CCtrlproxyApp::OnStatus() 
{
	CStatusDlg dlg();
	
}
