#if !defined(AFX_NETWORKSDLG_H__C3BB07E8_6224_4227_A90B_9CC2143F14E7__INCLUDED_)
#define AFX_NETWORKSDLG_H__C3BB07E8_6224_4227_A90B_9CC2143F14E7__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// NetworksDlg.h : header file
//

extern "C" {
#include "internals.h"
}

/////////////////////////////////////////////////////////////////////////////
// CNetworksDlg dialog

class CNetworksDlg : public CDialog
{
// Construction
public:
	CNetworksDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNetworksDlg)
	enum { IDD = IDD_NETWORKS };
	CButton	m_disconnect;
	CButton	m_connect;
	CListBox	m_networklist;
	CListBox	m_clientlist;
	CString	m_status;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetworksDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNetworksDlg)
	afx_msg void OnAdd();
	afx_msg void OnConnect();
	afx_msg void OnDel();
	afx_msg void OnEdit();
	afx_msg void OnKick();
	afx_msg void OnSelchangeNetworklist();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnDisconnect();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void UpdateNetworkList();
	struct network *curnetwork;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETWORKSDLG_H__C3BB07E8_6224_4227_A90B_9CC2143F14E7__INCLUDED_)
