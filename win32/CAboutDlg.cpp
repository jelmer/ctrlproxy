// CAboutDlg.cpp : implementation file
//

extern "C" {
#include "internals.h"
}
#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "CAboutDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog


CAboutDlg::CAboutDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAboutDlg::IDD, pParent)
{
	Create(IDD, pParent);
	//{{AFX_DATA_INIT(CAboutDlg)
	m_license = _T("foo");
	//}}AFX_DATA_INIT
	
}


void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Control(pDX, IDC_LICENSE, m_licenseControl);
	DDX_Text(pDX, IDC_LICENSE, m_license);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg message handlers

BOOL CAboutDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	char *license_path = g_build_filename(get_shared_path(), "COPYING", NULL);
	char buf[255];
	TRY {
	CFile f(license_path, CFile::modeRead);
	while(f.Read(buf, sizeof(buf)) > 0) m_license+=(buf);
	m_license+="\n";

	g_free(license_path);
	} CATCH (CFileException, e) {
		m_license = "Published under the GPL. See http://www.gnu.org/licenses/gpl.txt";
	}
	END_CATCH

	
	m_licenseControl.SetWindowText(m_license);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CAboutDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);
}
