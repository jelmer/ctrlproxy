// TrayNot.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "TrayNot.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTrayNot

CTrayNot::CTrayNot(CPropertySheet *s)
{
	dlg = s;
	Create(IDD_PHONY);
	EnableWindow(FALSE);
	
	NOTIFYICONDATA dat;
	dat.cbSize = sizeof(NOTIFYICONDATA);
	dat.hWnd = m_hWnd;
	dat.uFlags = NIF_ICON + NIF_TIP + NIF_MESSAGE;
	dat.hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	dat.uCallbackMessage = CTRLPROXY_TRAY_ICON;
	dat.uID = 1;
	strcpy(dat.szTip, "CtrlProxy manager");
	Shell_NotifyIcon(NIM_ADD, &dat);
}

CTrayNot::~CTrayNot()
{
	NOTIFYICONDATA tnid; 
 
    tnid.cbSize = sizeof(NOTIFYICONDATA); 
    tnid.hWnd = m_hWnd; 
    tnid.uID = 1; 
         
    Shell_NotifyIcon(NIM_DELETE, &tnid); 
}


BEGIN_MESSAGE_MAP(CTrayNot, CDialog)
	//{{AFX_MSG_MAP(CTrayNot)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		ON_MESSAGE (CTRLPROXY_TRAY_ICON, OnSysTrayIconClick)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTrayNot message handlers

afx_msg LONG CTrayNot::OnSysTrayIconClick (WPARAM wParam, LPARAM lParam)
{
 switch (lParam)
    {
              case WM_LBUTTONDOWN:
					dlg->DoModal();
					break;
              case WM_RBUTTONDOWN:
                   ShowQuickMenu ();
                   break ;
    }
 	return 0;
}

void CTrayNot::ShowQuickMenu()
{
   POINT CurPos;

   CMenu qmenu;
   qmenu.LoadMenu(IDR_POPUP);
   
   GetCursorPos (&CurPos);

   CMenu *submenu = qmenu.GetSubMenu(0);

   SetForegroundWindow();
   // Display the menu. This menu is a popup loaded elsewhere.

	submenu->TrackPopupMenu (TPM_RIGHTBUTTON | TPM_RIGHTALIGN,
                   CurPos.x,
                   CurPos.y,this);
}
