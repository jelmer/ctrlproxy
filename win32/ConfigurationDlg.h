#if !defined(AFX_CONFIGURATIONDLG_H__0886BC3E_AEFB_46A5_B946_28B27CD62B76__INCLUDED_)
#define AFX_CONFIGURATIONDLG_H__0886BC3E_AEFB_46A5_B946_28B27CD62B76__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigurationDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigurationDlg dialog

class CConfigurationDlg : public CDialog
{
// Construction
public:
	CConfigurationDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CConfigurationDlg)
	enum { IDD = IDD_CONFIGURATION };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigurationDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigurationDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGURATIONDLG_H__0886BC3E_AEFB_46A5_B946_28B27CD62B76__INCLUDED_)
