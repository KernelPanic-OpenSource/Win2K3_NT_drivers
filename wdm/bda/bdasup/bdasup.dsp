# Microsoft Developer Studio Project File - Name="bdasup" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=bdasup - Win32 ia64 Checked
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "bdasup.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "bdasup.mak" CFG="bdasup - Win32 ia64 Checked"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "bdasup - Win32 x86 Free" (based on "Win32 (x86) External Target")
!MESSAGE "bdasup - Win32 x86 Checked" (based on "Win32 (x86) External Target")
!MESSAGE "bdasup - Win32 ia64 Free" (based on "Win32 (x86) External Target")
!MESSAGE "bdasup - Win32 ia64 Checked" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "bdasup"
# PROP Scc_LocalPath "."

!IF  "$(CFG)" == "bdasup - Win32 x86 Free"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "x86 Free"
# PROP BASE Intermediate_Dir "x86 Free"
# PROP BASE Cmd_Line "cd /d g:\ntw&&tools\razzle.cmd free&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP BASE Rebuild_Opt "/c"
# PROP BASE Target_File "obj\i386\bdasup.sys"
# PROP BASE Bsc_Name "bdasup.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "x86 Free"
# PROP Intermediate_Dir "x86 Free"
# PROP Cmd_Line "cd /d %SDXROOT%&&tools\razzle.cmd free&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP Rebuild_Opt "/c"
# PROP Target_File "obj\i386\bdasup.sys"
# PROP Bsc_Name "bdasup.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "bdasup - Win32 x86 Checked"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "x86 Checked"
# PROP BASE Intermediate_Dir "x86 Checked"
# PROP BASE Cmd_Line "cd /d g:\ntw&&tools\razzle.cmd no_opt&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP BASE Rebuild_Opt "/c"
# PROP BASE Target_File "objd\i386\bdasup.sys"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "x86 Checked"
# PROP Intermediate_Dir "x86 Checked"
# PROP Cmd_Line "cd /d %SDXROOT%&&tools\razzle.cmd no_opt&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP Rebuild_Opt "/c"
# PROP Target_File "objd\i386\bdasup.sys"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "bdasup - Win32 ia64 Free"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ia64 Free"
# PROP BASE Intermediate_Dir "ia64 Free"
# PROP BASE Cmd_Line "cd /d g:\ntw&&tools\razzle.cmd free&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP BASE Rebuild_Opt "/c"
# PROP BASE Target_File "obj\i386\bdasup.sys"
# PROP BASE Bsc_Name "bdasup.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ia64 Free"
# PROP Intermediate_Dir "ia64 Free"
# PROP Cmd_Line "cd /d %SDXROOT%&&tools\razzle.cmd Win64 free&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP Rebuild_Opt "/c"
# PROP Target_File "obj\ia64\bdasup.sys"
# PROP Bsc_Name "bdasup.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "bdasup - Win32 ia64 Checked"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ia64 Checked"
# PROP BASE Intermediate_Dir "ia64 Checked"
# PROP BASE Cmd_Line "cd /d g:\ntw&&tools\razzle.cmd no_opt&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP BASE Rebuild_Opt "/c"
# PROP BASE Target_File "objd\i386\bdasup.sys"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ia64 Checked"
# PROP Intermediate_Dir "ia64 Checked"
# PROP Cmd_Line "cd /d %SDXROOT%&&tools\razzle.cmd Win64 no_opt&&cd drivers\wdm\bda\bdasup&&build /Z"
# PROP Rebuild_Opt "/c"
# PROP Target_File "objd\ia64\bdasup.sys"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "bdasup - Win32 x86 Free"
# Name "bdasup - Win32 x86 Checked"
# Name "bdasup - Win32 ia64 Free"
# Name "bdasup - Win32 ia64 Checked"

!IF  "$(CFG)" == "bdasup - Win32 x86 Free"

!ELSEIF  "$(CFG)" == "bdasup - Win32 x86 Checked"

!ELSEIF  "$(CFG)" == "bdasup - Win32 ia64 Free"

!ELSEIF  "$(CFG)" == "bdasup - Win32 ia64 Checked"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\BdaSup.def
# End Source File
# Begin Source File

SOURCE=.\BdaSup.rc
# End Source File
# Begin Source File

SOURCE=.\bdatopgy.c
# End Source File
# Begin Source File

SOURCE=.\objdesc.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BdaSupI.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\makefile
# End Source File
# Begin Source File

SOURCE=.\makefile.inc
# End Source File
# Begin Source File

SOURCE=.\sources
# End Source File
# End Target
# End Project
