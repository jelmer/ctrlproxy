#if !defined(AFX_NEWNETWORKDIALOG_H__8F6B4CBD_6B4E_4623_979A_EB61283489AA__INCLUDED_)
#define AFX_NEWNETWORKDIALOG_H__8F6B4CBD_6B4E_4623_979A_EB61283489AA__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// NewNetworkDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNewNetworkDlg dialog

class CNewNetworkDlg : public CDialog
{
// Construction
public:
	CNewNetworkDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewNetworkDlg)
	enum { IDD = IDD_NEW_NETWORK };
	CString	NetworkName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewNetworkDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNewNetworkDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NEWNETWORKDIALOG_H__8F6B4CBD_6B4E_4623_979A_EB61283489AA__INCLUDED_)
