// ctrlproxy.h : main header file for the CTRLPROXY application
//

#if !defined(AFX_CTRLPROXY_H__086E5594_617D_43E8_9D08_C96E72C5CEAC__INCLUDED_)
#define AFX_CTRLPROXY_H__086E5594_617D_43E8_9D08_C96E72C5CEAC__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "traynot.h"
/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyApp:
// See ctrlproxy.cpp for the implementation of this class
//

class CCtrlproxyApp : public CWinApp
{
public:
	CCtrlproxyApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCtrlproxyApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual BOOL OnIdle(LONG lCount);
	virtual BOOL SaveAllModified();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CCtrlproxyApp)
	afx_msg void OnExit();
	afx_msg void OnShow();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	private:
	CPropertySheet *dlg;
	CTrayNot *not;

};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CTRLPROXY_H__086E5594_617D_43E8_9D08_C96E72C5CEAC__INCLUDED_)
