// ctrlproxyDlg.h : header file
//

#if !defined(AFX_CTRLPROXYDLG_H__CF245304_D765_4E7B_80C6_7777E5247329__INCLUDED_)
#define AFX_CTRLPROXYDLG_H__CF245304_D765_4E7B_80C6_7777E5247329__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ConfigurationDlg.h"
#include "CPluginsDlg.h"
#include "CLogDlg.h"
#include "CNetworksDlg.h"
#include "CStatusDlg.h"
#include "CAboutDlg.h"

#define TAB_STATUS 0
#define TAB_NETWORKS 1
#define TAB_PLUGINS 2
#define TAB_CONFIGURATION 3
#define TAB_LOG 4
#define TAB_ABOUT 5
#define NUM_TABS 6

#define CTRLPROXY_TRAY_ICON WM_USER+1


/////////////////////////////////////////////////////////////////////////////
// CCtrlproxyDlg dialog

class CCtrlproxyDlg : public CDialog
{
// Construction
public:
	CCtrlproxyDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CCtrlproxyDlg)
	enum { IDD = IDD_CTRLPROXY_DIALOG };
	CTabCtrl	m_Tab;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCtrlproxyDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CCtrlproxyDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LONG OnSysTrayIconClick (WPARAM wParam, LPARAM lParam);
	afx_msg void OnClose();
	afx_msg void OnExit();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	private:
	CDialog *tabs[6];
	void UpdateVisibleWindow();
	void ShowQuickMenu();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CTRLPROXYDLG_H__CF245304_D765_4E7B_80C6_7777E5247329__INCLUDED_)
