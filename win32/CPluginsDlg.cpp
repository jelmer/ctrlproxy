// CPluginsDlg.cpp : implementation file
//

extern "C" {
#include "internals.h"
#include <direct.h>
}

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "CPluginsDlg.h"
#include "commdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPluginsDlg dialog


CPluginsDlg::CPluginsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPluginsDlg::IDD, pParent)
{
	Create(IDD, pParent);
	//{{AFX_DATA_INIT(CPluginsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CPluginsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPluginsDlg)
	DDX_Control(pDX, IDC_PLUGIN_LIST, m_list);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPluginsDlg, CDialog)
	//{{AFX_MSG_MAP(CPluginsDlg)
	ON_BN_CLICKED(IDC_ADD_PLUGIN, OnAddPlugin)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPluginsDlg message handlers

void CPluginsDlg::OnAddPlugin() 
{
	chdir(get_modules_path());
	
	CFileDialog dlg(TRUE,NULL,NULL,OFN_OVERWRITEPROMPT,"CtrlProxy plugins (*.dll)|*.dll|All Files (*.*)|*.*||",this);
	if(dlg.DoModal() == IDCANCEL) return;

	CString f = dlg.GetPathName();
	const char *filename = f;
	TRACE(TEXT("Opening %s\n"), filename);

	xmlNodePtr cur = xmlNewNode(NULL, (xmlChar *)"plugin");
	xmlSetProp(cur, (xmlChar *)"file", (xmlChar *)filename);
	xmlAddChild(config_node_plugins(), cur);

	load_plugin(cur);
}


void CPluginsDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);

	if(bShow)UpdatePluginList();
	
}

void CPluginsDlg::UpdatePluginList()
{
	m_list.DeleteAllItems();
	xmlNodePtr cur = config_node_plugins()->xmlChildrenNode;
	while(cur) {
		if(!strcmp((char *)cur->name, "plugin")) { 
			char *name = (char *)xmlGetProp(cur, (xmlChar *)"file");
			m_list.InsertItem(0, name);		
			xmlFree(name);
		}
		cur = cur->next;
	}

}

BOOL CPluginsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_list.InsertColumn(0, "Name", LVCFMT_LEFT, 200);
	m_list.InsertColumn(1, "Loaded",LVCFMT_RIGHT, 60);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
