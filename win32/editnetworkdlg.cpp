// EditNetworkDialog.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "EditNetworkDlg.h"
#include "EditServer.h"
#include "EditListener.h"
#include "EditChannel.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEditNetworkDlg dialog


CEditNetworkDlg::CEditNetworkDlg(CWnd* pParent, xmlNodePtr p /*=NULL*/)
	: CDialog(CEditNetworkDlg::IDD, pParent)
{
	node = p;
	listen = xmlFindChildByElementName(node, (xmlChar *)"listen");
	servers = xmlFindChildByElementName(node, (xmlChar *)"servers");
	if(!listen) {
		listen = xmlNewNode(NULL, (xmlChar *)"listen");
		xmlAddChild(node, listen);
	}

	if(!servers) {
		servers = xmlNewNode(NULL, (xmlChar *)"servers");
		xmlAddChild(node, servers);
	}
	
	//{{AFX_DATA_INIT(CEditNetworkDlg)
	//}}AFX_DATA_INIT
}


void CEditNetworkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditNetworkDlg)
	DDX_Control(pDX, IDC_EDIT_CHANNEL, m_edit_channel);
	DDX_Control(pDX, IDC_ADD_SERVER, m_add_server);
	DDX_Control(pDX, IDC_ADD_LISTENER, m_add_listener);
	DDX_Control(pDX, IDC_ADD_CHANNEL, m_add_channel);
	DDX_Control(pDX, IDC_REMOVE_SERVER, m_remove_server);
	DDX_Control(pDX, IDC_REMOVE_LISTENER, m_remove_listener);
	DDX_Control(pDX, IDC_REMOVE_CHANNEL, m_remove_channel);
	DDX_Control(pDX, IDC_CHANNELLIST, m_channellist);
	DDX_Control(pDX, IDC_LISTENERLIST, m_listenerlist);
	DDX_Control(pDX, IDC_NAME, m_name);
	DDX_Control(pDX, IDC_USERNAME, m_username);
	DDX_Control(pDX, IDC_SSL, m_ssl);
	DDX_Control(pDX, IDC_AUTOCONNECT, m_autoconnect);
	DDX_Control(pDX, IDC_SERVERPASS, m_serverpass);
	DDX_Control(pDX, IDC_SERVERLIST, m_serverlist);
	DDX_Control(pDX, IDC_NICKNAME, m_nickname);
	DDX_Control(pDX, IDC_FULLNAME, m_fullname);
	DDX_Control(pDX, IDC_CLIENTPASS, m_clientpass);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditNetworkDlg, CDialog)
	//{{AFX_MSG_MAP(CEditNetworkDlg)
	ON_BN_CLICKED(IDC_ADD_CHANNEL, OnAddChannel)
	ON_BN_CLICKED(IDC_ADD_LISTENER, OnAddListener)
	ON_BN_CLICKED(IDC_ADD_SERVER, OnAddServer)
	ON_BN_CLICKED(IDC_REMOVE_CHANNEL, OnRemoveChannel)
	ON_BN_CLICKED(IDC_REMOVE_LISTENER, OnRemoveListener)
	ON_BN_CLICKED(IDC_REMOVE_SERVER, OnRemoveServer)
	ON_BN_CLICKED(IDC_EDIT_CHANNEL, OnEditChannel)
	ON_LBN_SELCHANGE(IDC_CHANNELLIST, OnSelchangeChannellist)
	ON_LBN_SELCHANGE(IDC_LISTENERLIST, OnSelchangeListenerlist)
	ON_LBN_SELCHANGE(IDC_SERVERLIST, OnSelchangeServerlist)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditNetworkDlg message handlers

BOOL CEditNetworkDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
		char *tmp;
#define SETFIELD(a,b) {tmp = (char *)xmlGetProp(node, (xmlChar *)a); if(tmp)b.SetWindowText(tmp); xmlFree(tmp); }
	SETFIELD("name", m_name);
	SETFIELD("pass", m_serverpass);
	SETFIELD("username", m_username);
	SETFIELD("nick", m_nickname);
	SETFIELD("fullname", m_fullname);
	SETFIELD("client_pass", m_clientpass);
#undef SETFIELD
	tmp = (char *)xmlGetProp(node, (xmlChar *)"autoconnect");
	if(tmp && atol(tmp)) m_autoconnect.SetCheck(1);
	xmlFree(tmp);
	UpdateChannelList();
	UpdateServerList();
	UpdateListenerList();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CEditNetworkDlg::OnOK() 
{
	CString tmp; const char *tmp1;
#define SETFIELD(a,b) { b.GetWindowText(tmp); tmp1 = tmp; xmlSetProp(node, (xmlChar *)a, (xmlChar *) tmp1); }
	SETFIELD("name", m_name);
	SETFIELD("pass", m_serverpass);
	SETFIELD("username", m_username);
	SETFIELD("nick", m_nickname);
	SETFIELD("fullname", m_fullname);
	SETFIELD("client_pass", m_clientpass);

	xmlSetProp(node, (xmlChar *)"autoconnect", (xmlChar *)(m_autoconnect.GetCheck()?"1":"0"));

	
	CDialog::OnOK();
}

void CEditNetworkDlg::OnAddChannel() 
{
	xmlNodePtr cur = xmlNewNode(NULL, (xmlChar *)"channel");
	CEditChannel dlg(this, cur);
	if(dlg.DoModal() == IDCANCEL) return;
	xmlAddChild(node, cur);

	UpdateChannelList();
}

void CEditNetworkDlg::OnAddListener() 
{
	xmlNodePtr cur = xmlNewNode(NULL, (xmlChar *)"ipv4");
	CEditListener dlg(this, cur);
	if(dlg.DoModal() == IDCANCEL) return;
	xmlAddChild(listen, cur);

	UpdateListenerList();
}

void CEditNetworkDlg::OnAddServer() 
{
	xmlNodePtr cur = xmlNewNode(NULL, (xmlChar *)"ipv4");
	CEditServer dlg(this,cur);
	if(dlg.DoModal() == IDCANCEL) return;
	xmlAddChild(servers, cur);

	UpdateServerList();
}

void CEditNetworkDlg::OnRemoveChannel() 
{
	xmlNodePtr cur = (xmlNodePtr) m_channellist.GetItemData(m_channellist.GetCurSel());

	if(!cur) return;

	xmlUnlinkNode(cur);
	xmlFreeNode(cur);

	UpdateChannelList();	
}

void CEditNetworkDlg::OnRemoveListener() 
{
	xmlNodePtr cur = (xmlNodePtr) m_listenerlist.GetItemData(m_listenerlist.GetCurSel());

	if(!cur) return;

	xmlUnlinkNode(cur);
	xmlFreeNode(cur);

	UpdateListenerList();
}

void CEditNetworkDlg::OnRemoveServer() 
{
	xmlNodePtr cur = (xmlNodePtr) m_serverlist.GetItemData(m_serverlist.GetCurSel());

	if(!cur) return;

	xmlUnlinkNode(cur);
	xmlFreeNode(cur);

	UpdateServerList();
}

void CEditNetworkDlg::UpdateServerList()
{
	m_serverlist.ResetContent();
	xmlNodePtr cur = servers->xmlChildrenNode;
	while(cur) {
		if(!strcmp((char *)cur->name, "ipv4")) {
			char *host = (char *)xmlGetProp(cur, (xmlChar *)"host");
			char *port = (char *)xmlGetProp(cur, (xmlChar *)"port");
			CString tmp;
			tmp.Format("%s:%s", host, port?port:"6667");
			int idx = m_serverlist.AddString(tmp);
			m_serverlist.SetItemData(idx, (unsigned long)cur);
			xmlFree(host);
			xmlFree(port);
		}
		cur = cur->next;
	}

	if(m_serverlist.GetCurSel() == LB_ERR) m_remove_server.EnableWindow(FALSE);
}

void CEditNetworkDlg::UpdateListenerList()
{
	m_listenerlist.ResetContent();
	xmlNodePtr cur = listen->xmlChildrenNode;
	while(cur) {
		if(!strcmp((char *)cur->name, "ipv4")) {
			char *port = (char *)xmlGetProp(cur, (xmlChar *)"port");
			CString tmp;
			tmp.Format("%s", port?port:"6667");
			int idx = m_listenerlist.AddString(tmp);
			m_listenerlist.SetItemData(idx, (unsigned long)cur);
			xmlFree(port);
		}
		cur = cur->next;
	}

	if(m_listenerlist.GetCurSel() == LB_ERR) m_remove_listener.EnableWindow(FALSE);
}

void CEditNetworkDlg::UpdateChannelList()
{
	m_channellist.ResetContent();
	xmlNodePtr cur = node->xmlChildrenNode;
	while(cur) {
		if(!strcmp((char *)cur->name, "channel")) {
			char *name = (char *)xmlGetProp(cur, (xmlChar *)"name");
			CString tmp;
			tmp.Format("%s", name);
			int idx = m_channellist.AddString(tmp);
			m_channellist.SetItemData(idx, (unsigned long)cur);
			xmlFree(name);
		}

		cur = cur->next;
	}

	if(m_channellist.GetCurSel() == LB_ERR) {
		m_remove_channel.EnableWindow(FALSE);
		m_edit_channel.EnableWindow(FALSE);
	}
}

void CEditNetworkDlg::OnEditChannel() 
{
	xmlNodePtr cur = (xmlNodePtr) m_channellist.GetItemData(m_channellist.GetCurSel());

	if(!cur) return;
	CEditChannel dlg(this, cur);
	if(dlg.DoModal() == IDCANCEL) return;

	UpdateServerList();
}

void CEditNetworkDlg::OnSelchangeChannellist() 
{
	m_remove_channel.EnableWindow(m_channellist.GetCurSel() == LB_ERR?FALSE:TRUE);
	m_edit_channel.EnableWindow(m_channellist.GetCurSel() == LB_ERR?FALSE:TRUE);
}

void CEditNetworkDlg::OnSelchangeListenerlist() 
{
	m_remove_listener.EnableWindow(m_listenerlist.GetCurSel() == LB_ERR?FALSE:TRUE);
	
}

void CEditNetworkDlg::OnSelchangeServerlist() 
{
	m_remove_server.EnableWindow(m_serverlist.GetCurSel() == LB_ERR?FALSE:TRUE);
	
}

void CEditNetworkDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);

	if(!bShow)return;
	
	UpdateChannelList();
	UpdateServerList();
	UpdateListenerList();
}
