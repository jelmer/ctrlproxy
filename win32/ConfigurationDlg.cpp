// ConfigurationDlg.cpp : implementation file
//

extern "C" {
#include "internals.h"
}
#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "ConfigurationDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigurationDlg dialog


CConfigurationDlg::CConfigurationDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigurationDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigurationDlg)
	//}}AFX_DATA_INIT
}


void CConfigurationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigurationDlg)
	DDX_Control(pDX, IDC_CONFTREE, m_Tree);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigurationDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigurationDlg)
	ON_BN_CLICKED(IDC_SAVECONFIG, OnSaveConfig)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigurationDlg message handlers

void CConfigurationDlg::OnSaveConfig() 
{
	save_configuration();
}

void CConfigurationDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);
	if(bShow) UpdateTree();
}

void CConfigurationDlg::fillinchildren(HTREEITEM i, xmlNodePtr cur)
{
	HTREEITEM t = m_Tree.InsertItem((char *)cur->name, i);
	xmlNodePtr c = cur->xmlChildrenNode;
	while(c) {
		fillinchildren(t, c);
		c = c->next;
	}
}

BOOL CConfigurationDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_Tree.m_hWnd = m_hWnd;
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CConfigurationDlg::UpdateTree()
{
	if(!::IsWindow(m_Tree.m_hWnd)) return;
	m_Tree.DeleteAllItems();

	fillinchildren(TVI_ROOT, config_node_root());

}
