#if !defined(AFX_EDITLISTENER_H__5D8FEE2B_3851_46C1_94E0_D002E93FFD3B__INCLUDED_)
#define AFX_EDITLISTENER_H__5D8FEE2B_3851_46C1_94E0_D002E93FFD3B__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// EditListener.h : header file
//

extern "C" {
#include "internals.h"
}

/////////////////////////////////////////////////////////////////////////////
// CEditListener dialog

class CEditListener : public CDialog
{
// Construction
public:
	CEditListener(CWnd* pParent, xmlNodePtr );   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditListener)
	enum { IDD = IDD_EDIT_LISTENER };
	CEdit	m_port;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditListener)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	xmlNodePtr node;

	// Generated message map functions
	//{{AFX_MSG(CEditListener)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITLISTENER_H__5D8FEE2B_3851_46C1_94E0_D002E93FFD3B__INCLUDED_)
