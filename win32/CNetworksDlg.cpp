// NetworksDlg.cpp : implementation file
//

extern "C" {
#include "internals.h"
#include <string.h>
}
#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "CNetworksDlg.h"
#include "NewNetworkDlg.h"
#include "EditNetworkDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetworksDlg dialog


CNetworksDlg::CNetworksDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNetworksDlg::IDD, pParent)
{
	Create(IDD, pParent);
	//{{AFX_DATA_INIT(CNetworksDlg)
	m_status = _T("");
	//}}AFX_DATA_INIT
}


void CNetworksDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNetworksDlg)
	DDX_Control(pDX, IDC_DISCONNECT, m_disconnect);
	DDX_Control(pDX, IDC_CONNECT, m_connect);
	DDX_Control(pDX, IDC_NETWORKLIST, m_networklist);
	DDX_Control(pDX, IDC_CLIENTLIST, m_clientlist);
	DDX_Text(pDX, IDC_STATUS, m_status);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNetworksDlg, CDialog)
	//{{AFX_MSG_MAP(CNetworksDlg)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_CONNECT, OnConnect)
	ON_BN_CLICKED(IDC_DEL, OnDel)
	ON_BN_CLICKED(IDC_EDIT, OnEdit)
	ON_BN_CLICKED(IDC_KICK, OnKick)
	ON_LBN_SELCHANGE(IDC_NETWORKLIST, OnSelchangeNetworklist)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_DISCONNECT, OnDisconnect)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetworksDlg message handlers

void CNetworksDlg::OnAdd() 
{
	CNewNetworkDlg *dlg = new CNewNetworkDlg(this);
	if(dlg->DoModal() == IDCANCEL) return;
	CString n = dlg->NetworkName;
	xmlNodePtr cur;

	/* Add node to networks node with specified name */
	cur = xmlNewNode(NULL, (xmlChar *)"network");
	const char *tmp = n;
	xmlSetProp(cur, (xmlChar *)"name", (xmlChar *)tmp);
	xmlAddChild(config_node_networks(), cur);

	/* Add a 'servers' element */
	xmlAddChild(cur, xmlNewNode(NULL, (xmlChar *)"servers"));

	UpdateNetworkList();
}

void CNetworksDlg::OnConnect() 
{
	xmlNodePtr cur = (xmlNodePtr)m_networklist.GetItemData(m_networklist.GetCurSel());
	connect_network(cur);
}

void CNetworksDlg::OnDel() 
{
	
	
}

void CNetworksDlg::OnEdit() 
{
	int cursel = m_networklist.GetCurSel();
	CEditNetworkDlg *dlg = new CEditNetworkDlg(this, (xmlNodePtr)m_networklist.GetItemData(cursel));	
	dlg->DoModal();
}

void CNetworksDlg::OnKick() 
{
	struct client *c = (struct client *)m_clientlist.GetItemData(m_clientlist.GetCurSel());
	disconnect_client(c);
}

void CNetworksDlg::OnSelchangeNetworklist() 
{
	xmlNodePtr cur = (xmlNodePtr)m_networklist.GetItemData(m_networklist.GetCurSel());
	curnetwork = find_network_by_xml(cur);
	m_disconnect.EnableWindow(curnetwork?TRUE:FALSE);
	m_connect.EnableWindow(curnetwork?FALSE:TRUE);
	
}

void CNetworksDlg::UpdateNetworkList()
{
	m_networklist.ResetContent();
	if(!config_node_networks()) return;
	xmlNodePtr n = config_node_networks()->xmlChildrenNode;
	while(n) {
		if(!strcmp((char *)n->name, "network")) {
			char *name = (char *)xmlGetProp(n, (xmlChar *)"name");
			if(!name)name = g_strdup("<UNNAMED>");
			int idx = m_networklist.AddString(name);
			xmlFree(name);
			m_networklist.SetItemData(idx, (unsigned long)n);
		}

		n = n->next;
	}
}

void CNetworksDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);

	if(bShow)UpdateNetworkList();
	
}

void CNetworksDlg::OnDisconnect() 
{
	if(curnetwork)close_network(curnetwork);
}
