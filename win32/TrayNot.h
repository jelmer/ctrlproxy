#if !defined(AFX_TRAYNOT_H__9FEB5713_4D2B_4685_B2E2_2BB1F1FC7919__INCLUDED_)
#define AFX_TRAYNOT_H__9FEB5713_4D2B_4685_B2E2_2BB1F1FC7919__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// TrayNot.h : header file
//

#define CTRLPROXY_TRAY_ICON WM_USER+1

/////////////////////////////////////////////////////////////////////////////
// CTrayNot window

class CTrayNot : public CDialog
{
// Construction
public:
	CTrayNot(CPropertySheet *s);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTrayNot)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTrayNot();

	// Generated message map functions
protected:
	//{{AFX_MSG(CTrayNot)
		// NOTE - the ClassWizard will add and remove member functions here.
	afx_msg LONG OnSysTrayIconClick (WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()			
	void ShowQuickMenu();
	CPropertySheet *dlg;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TRAYNOT_H__9FEB5713_4D2B_4685_B2E2_2BB1F1FC7919__INCLUDED_)
