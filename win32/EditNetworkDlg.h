#if !defined(AFX_EDITNETWORKDIALOG_H__C5A3B8CD_B44C_445C_BEB5_B7781EFF4536__INCLUDED_)
#define AFX_EDITNETWORKDIALOG_H__C5A3B8CD_B44C_445C_BEB5_B7781EFF4536__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

extern "C" {
#include "internals.h"
}
// EditNetworkDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CEditNetworkDlg dialog

class CEditNetworkDlg : public CDialog
{
// Construction
public:
	CEditNetworkDlg(CWnd* pParent, xmlNodePtr p);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditNetworkDlg)
	enum { IDD = IDD_EDIT_NETWORK };
	CButton	m_edit_channel;
	CButton	m_add_server;
	CButton	m_add_listener;
	CButton	m_add_channel;
	CButton	m_remove_server;
	CButton	m_remove_listener;
	CButton	m_remove_channel;
	CListBox	m_channellist;
	CListBox	m_listenerlist;
	CEdit	m_name;
	CEdit	m_username;
	CButton	m_ssl;
	CButton	m_autoconnect;
	CEdit	m_serverpass;
	CListBox	m_serverlist;
	CEdit	m_nickname;
	CEdit	m_fullname;
	CEdit	m_clientpass;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditNetworkDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	xmlNodePtr node;
	xmlNodePtr listen;
	xmlNodePtr servers;
	// Generated message map functions
	//{{AFX_MSG(CEditNetworkDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnAddChannel();
	afx_msg void OnAddListener();
	afx_msg void OnAddServer();
	afx_msg void OnRemoveChannel();
	afx_msg void OnRemoveListener();
	afx_msg void OnRemoveServer();
	afx_msg void OnEditChannel();
	afx_msg void OnSelchangeChannellist();
	afx_msg void OnSelchangeListenerlist();
	afx_msg void OnSelchangeServerlist();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void UpdateChannelList();
	void UpdateListenerList();
	void UpdateServerList();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITNETWORKDIALOG_H__C5A3B8CD_B44C_445C_BEB5_B7781EFF4536__INCLUDED_)
