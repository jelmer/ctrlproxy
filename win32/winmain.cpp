#include <stdio.h>
#include "resource.h"
#include <afxwin.h>
#include <shellapi.h>

class CtrlProxyApp : public CWinApp {
public:
	CtrlProxyApp() : CWinApp() {

	}

	virtual BOOL InitInstance() {
		NOTIFYICONDATA dat;
		dat.cbSize = sizeof(NOTIFYICONDATA);
		dat.hWnd = NULL; /*FIXME*/
		dat.uFlags = NIF_ICON + NIF_TIP;
		dat.hIcon = LoadIcon(IDI_ICON1);
		strcpy(dat.szTip, "CtrlProxy manager");

		Shell_NotifyIcon(NIM_ADD, &dat);
		return TRUE;
	}

	virtual BOOL Run() { while(TRUE); return TRUE; }
};

CtrlProxyApp theApp;
