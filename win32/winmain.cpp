#include <stdio.h>
#include "resource.h"
#include <afxwin.h>
#include <shellapi.h>


#define CTRLPROXY_TRAY_ICON WM_APP+1

void ShowContextMenu(HWND hwnd) { 
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, "About");
	SetForegroundWindow(hwnd);
	TrackPopupMenu(menu.Detach(), TPM_LEFTALIGN, 0, 0, 0, hwnd, NULL);

} 

BOOL CALLBACK MyDlgProc(HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
		case CTRLPROXY_TRAY_ICON:
			switch(lParam)
			{
				case WM_LBUTTONDBLCLK:
					ShowWindow(hWnd, SW_RESTORE);
					break;
				case WM_RBUTTONDOWN:
				case WM_CONTEXTMENU:
				   ShowContextMenu(hWnd);
					break;
			}
			break;
	}
	return TRUE;
}


class CtrlProxyApp : public CWinApp {
public:
	CtrlProxyApp() : CWinApp() {

	}

	virtual BOOL InitInstance() {
		NOTIFYICONDATA dat;
		dat.cbSize = sizeof(NOTIFYICONDATA);
		dat.hWnd = CreateDialog( m_hInstance, MAKEINTRESOURCE(IDD_DEFAULT),NULL,(DLGPROC)MyDlgProc );
		dat.uFlags = NIF_ICON + NIF_TIP;
		dat.hIcon = LoadIcon(IDI_ICON1);
		dat.uCallbackMessage = CTRLPROXY_TRAY_ICON;
		strcpy(dat.szTip, "CtrlProxy manager");
		Shell_NotifyIcon(NIM_ADD, &dat);
		return TRUE;
	}

	virtual BOOL Run() { while(TRUE); return TRUE; }
};

CtrlProxyApp theApp;
