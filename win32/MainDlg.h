#if !defined(AFX_MAINDLG_H__B53DDB11_F0BA_4EE2_B8FE_B6F8D0D388C7__INCLUDED_)
#define AFX_MAINDLG_H__B53DDB11_F0BA_4EE2_B8FE_B6F8D0D388C7__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// MainDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMainDlg

class CMainDlg : public CPropertySheet
{
	DECLARE_DYNAMIC(CMainDlg)

// Construction
public:
	CMainDlg();
	CMainDlg(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CMainDlg(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainDlg)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainDlg();

	// Generated message map functions
protected:
	//{{AFX_MSG(CMainDlg)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINDLG_H__B53DDB11_F0BA_4EE2_B8FE_B6F8D0D388C7__INCLUDED_)
