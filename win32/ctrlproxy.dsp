# Microsoft Developer Studio Project File - Name="ctrlproxy" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ctrlproxy - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ctrlproxy.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ctrlproxy.mak" CFG="ctrlproxy - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ctrlproxy - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ctrlproxy - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ctrlproxy - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I ".." /I "c:\dev\include" /I "c:\dev\include\glib-2.0" /I "c:\dev\lib\glib-2.0\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /YX"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 asprintf.lib libxml2.lib iconv.lib intl.lib gobject-2.0.lib gmodule-2.0.lib glib-2.0.lib ws2_32.lib /nologo /subsystem:windows /machine:I386 /libpath:"c:\dev\lib" /libpath:"Debug"

!ELSEIF  "$(CFG)" == "ctrlproxy - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I ".." /I "c:\dev\include" /I "c:\dev\include\glib-2.0" /I "c:\dev\lib\glib-2.0\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /YX"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 asprintf.lib libxml2.lib iconv.lib intl.lib gobject-2.0.lib gmodule-2.0.lib glib-2.0.lib ws2_32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"c:\dev\lib"

!ENDIF 

# Begin Target

# Name "ctrlproxy - Win32 Release"
# Name "ctrlproxy - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CAboutDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\CLogDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\CNetworksDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\config.c
# End Source File
# Begin Source File

SOURCE=.\ConfigurationDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\CPluginsDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\CStatusDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ctrlproxy.rc

!IF  "$(CFG)" == "ctrlproxy - Win32 Release"

!ELSEIF  "$(CFG)" == "ctrlproxy - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ctrlproxyapp.cpp
# End Source File
# Begin Source File

SOURCE=.\ctrlproxyDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\EditChannel.cpp
# End Source File
# Begin Source File

SOURCE=.\EditListener.cpp
# End Source File
# Begin Source File

SOURCE=.\EditNetworkDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\EditServer.cpp
# End Source File
# Begin Source File

SOURCE=..\hooks.c
# End Source File
# Begin Source File

SOURCE=..\line.c
# End Source File
# Begin Source File

SOURCE=..\linestack.c
# End Source File
# Begin Source File

SOURCE=.\NewNetworkDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\plugins.c
# End Source File
# Begin Source File

SOURCE=..\server.c
# End Source File
# Begin Source File

SOURCE=.\snprintf.c
# End Source File
# Begin Source File

SOURCE=..\state.c
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=..\transport.c
# End Source File
# Begin Source File

SOURCE=..\util.c
# End Source File
# Begin Source File

SOURCE=.\winhelp.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\CAboutDlg.h
# End Source File
# Begin Source File

SOURCE=.\CLogDlg.h
# End Source File
# Begin Source File

SOURCE=.\CNetworksDlg.h
# End Source File
# Begin Source File

SOURCE=.\ConfigurationDlg.h
# End Source File
# Begin Source File

SOURCE=.\CPluginsDlg.h
# End Source File
# Begin Source File

SOURCE=.\CStatusDlg.h
# End Source File
# Begin Source File

SOURCE=..\ctrlproxy.h
# End Source File
# Begin Source File

SOURCE=.\ctrlproxyapp.h
# End Source File
# Begin Source File

SOURCE=.\ctrlproxyDlg.h
# End Source File
# Begin Source File

SOURCE=.\EditChannel.h
# End Source File
# Begin Source File

SOURCE=.\EditListener.h
# End Source File
# Begin Source File

SOURCE=.\EditNetworkDlg.h
# End Source File
# Begin Source File

SOURCE=.\EditServer.h
# End Source File
# Begin Source File

SOURCE=..\gettext.h
# End Source File
# Begin Source File

SOURCE=..\internals.h
# End Source File
# Begin Source File

SOURCE=..\irc.h
# End Source File
# Begin Source File

SOURCE=.\NewNetworkDlg.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\ctrlproxy.ico
# End Source File
# Begin Source File

SOURCE=.\res\ctrlproxy.rc2
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
