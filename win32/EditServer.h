#if !defined(AFX_EDITSERVER_H__93BAFC4A_A9E4_4F50_8574_3D550CDFA093__INCLUDED_)
#define AFX_EDITSERVER_H__93BAFC4A_A9E4_4F50_8574_3D550CDFA093__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// EditServer.h : header file
//

extern "C" {
#include "internals.h"
}

/////////////////////////////////////////////////////////////////////////////
// CEditServer dialog

class CEditServer : public CDialog
{
// Construction
public:
	CEditServer(CWnd* pParent, xmlNodePtr);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditServer)
	enum { IDD = IDD_EDIT_SERVER };
	CEdit	m_port;
	CEdit	m_host;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditServer)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	xmlNodePtr node;

	// Generated message map functions
	//{{AFX_MSG(CEditServer)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITSERVER_H__93BAFC4A_A9E4_4F50_8574_3D550CDFA093__INCLUDED_)
