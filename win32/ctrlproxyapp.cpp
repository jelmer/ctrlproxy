// ctrlproxy.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "traynot.h"
#include "maindlg.h"
extern "C" {
#include "internals.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
void register_log_function();
}

GMainLoop *main_loop = NULL;
char *modules_path = NULL, *shared_path = NULL;

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
	ON_COMMAND(IDM_SHOW, OnShow)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyApp construction

CCtrlproxyApp::CCtrlproxyApp()
{
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
	
	dlg = new CMainDlg();
	not = new CTrayNot(dlg);
	m_pMainWnd = not;

	SetRegistryKey("CtrlProxy");

	register_log_function();	

	add_filter_class("", -1);
	add_filter_class("client", 100);
	add_filter_class("replicate", 50);
	add_filter_class("log", 50);

	g_message(_("Logfile opened"));

	HKEY key;
	unsigned char databuf[256];
	DWORD len;
	RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\CtrlProxy", &key);
	
	if(RegQueryValueEx(key, "configfile", NULL, NULL, databuf, &len) == ERROR_SUCCESS) configuration_file = g_strdup((char *)databuf);
	else {
		MessageBox(NULL, "Unable to find registry key", "CtrlProxy", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	if(RegQueryValueEx(key, "modulesdir", NULL, NULL, databuf, &len) == ERROR_SUCCESS) modules_path = g_strdup((char *)databuf);
	else MessageBox(NULL, "Unable to find registry key for modules path", "CtrlProxy", MB_OK | MB_ICONEXCLAMATION);

	if(RegQueryValueEx(key, "shareddir", NULL, NULL, databuf, &len) == ERROR_SUCCESS) shared_path = g_strdup((char *)databuf);
	else MessageBox(NULL, "Unable to find registry key for shared path", "CtrlProxy", MB_OK | MB_ICONEXCLAMATION);
	
	readConfig(configuration_file);
	
	init_plugins();
	init_networks();

	initialized_hook_execute();

	main_loop = g_main_loop_new(NULL, FALSE);
	return TRUE;
}

const char *get_my_hostname() { return "localhost"; } 
const char *get_modules_path() { return modules_path; }
const char *get_shared_path() { return shared_path; }

const char *ctrlproxy_version() { 
	return VERSION;
}

void clean_exit()
{
	exit(0);
}	


int CCtrlproxyApp::ExitInstance() 
{
	GList *gl = get_network_list();
	while(gl) {
		struct network *n = (struct network *)gl->data;
		gl = gl->next;
		if(n) close_network(n);
	}

	delete not;
	
	return CWinApp::ExitInstance();
}

void CCtrlproxyApp::OnExit() 
{
	clean_exit();
	
}

void CCtrlproxyApp::OnShow() 
{
	dlg->DoModal();
}

BOOL CCtrlproxyApp::OnIdle(LONG lCount) 
{
	char msg[200];
	__try {
		g_main_context_iteration(g_main_loop_get_context(main_loop), FALSE);
	} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION?EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)  {
		{ 
		g_snprintf(msg, 200, "Plugin %s has caused an access violation and will be unloaded.", peek_plugin()->name);
		MessageBox(NULL, msg, "CtrlProxy", MB_OK | MB_ICONEXCLAMATION);
		unload_plugin(peek_plugin());
		}
	}

	CWinApp::OnIdle(lCount);

	return TRUE;
}

BOOL CCtrlproxyApp::SaveAllModified() 
{
	save_configuration();	
	return CWinApp::SaveAllModified();
}

