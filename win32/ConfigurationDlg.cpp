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
	DDX_Control(pDX, IDC_CONFTREE, m_tree);
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
	if(!bShow) return;

	::MessageBox(NULL, "FOO", "BAR", MB_OK);

	m_tree.DeleteAllItems();

	fillinchildren(TVI_ROOT, config_node_root());

}

void CConfigurationDlg::fillinchildren(HTREEITEM i, xmlNodePtr cur)
{
	HTREEITEM t = m_tree.InsertItem((char *)cur->name, i);
	xmlNodePtr c = cur->xmlChildrenNode;
	while(c) {
		fillinchildren(t, c);
		c = c->next;
	}
}
