// MainDlg.cpp : implementation file
//
#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "MainDlg.h"

#include "caboutdlg.h"
#include "cpluginsdlg.h"
#include "cnetworksdlg.h"
#include "cstatusdlg.h"
#include "configurationdlg.h"
#include "Clogdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainDlg

IMPLEMENT_DYNAMIC(CMainDlg, CPropertySheet)

CMainDlg::CMainDlg() :CPropertySheet() {
	AddPage(new CStatusDlg());
	AddPage(new CNetworksDlg());
	AddPage(new CPluginsDlg());
	AddPage(new CLogDlg());
	AddPage(new CConfigurationDlg());
	AddPage(new CAboutDlg());
}

CMainDlg::CMainDlg(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	CMainDlg();
	
}

CMainDlg::CMainDlg(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	CMainDlg();
	
}

CMainDlg::~CMainDlg()
{
}


BEGIN_MESSAGE_MAP(CMainDlg, CPropertySheet)
	//{{AFX_MSG_MAP(CMainDlg)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

