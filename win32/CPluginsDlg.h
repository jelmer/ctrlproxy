#if !defined(AFX_CPluginsDlg_H__7DCD55A5_5407_4A87_B30A_2B8751ECC18B__INCLUDED_)
#define AFX_CPluginsDlg_H__7DCD55A5_5407_4A87_B30A_2B8751ECC18B__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// CPluginsDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPluginsDlg dialog

class CPluginsDlg : public CDialog
{
// Construction
public:
	CPluginsDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPluginsDlg)
	enum { IDD = IDD_PLUGINS };
	CListCtrl	m_list;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPluginsDlg)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPluginsDlg)
	afx_msg void OnAddPlugin();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CPluginsDlg_H__7DCD55A5_5407_4A87_B30A_2B8751ECC18B__INCLUDED_)
