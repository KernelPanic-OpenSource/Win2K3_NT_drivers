$(OBJDIR)\config.obj $(OBJDIR)\config.lst: ..\config.c \
	$(WDMROOT)\ddk\inc\usb100.h ..\..\..\..\..\..\..\dev\inc\commctrl.h \
	..\..\..\..\..\..\..\dev\inc\commdlg.h \
	..\..\..\..\..\..\..\dev\inc\imm.h \
	..\..\..\..\..\..\..\dev\inc\mcx.h \
	..\..\..\..\..\..\..\dev\inc\mmsystem.h \
	..\..\..\..\..\..\..\dev\inc\netmpr.h \
	..\..\..\..\..\..\..\dev\inc\prsht.h \
	..\..\..\..\..\..\..\dev\inc\setupapi.h \
	..\..\..\..\..\..\..\dev\inc\shellapi.h \
	..\..\..\..\..\..\..\dev\inc\winbase.h \
	..\..\..\..\..\..\..\dev\inc\wincon.h \
	..\..\..\..\..\..\..\dev\inc\windef.h \
	..\..\..\..\..\..\..\dev\inc\windows.h \
	..\..\..\..\..\..\..\dev\inc\wingdi.h \
	..\..\..\..\..\..\..\dev\inc\winnetwk.h \
	..\..\..\..\..\..\..\dev\inc\winnls.h \
	..\..\..\..\..\..\..\dev\inc\winnt.h \
	..\..\..\..\..\..\..\dev\inc\winreg.h \
	..\..\..\..\..\..\..\dev\inc\winspool.h \
	..\..\..\..\..\..\..\dev\inc\winuser.h \
	..\..\..\..\..\..\..\dev\ntsdk\inc\devioctl.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\basetyps.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\cderr.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\cguid.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\dde.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\ddeml.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\dlgs.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\initguid.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\lzexpand.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\nb30.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\oaidl.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\objbase.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\objidl.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\ole.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\ole2.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\oleauto.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\oleidl.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\POPPACK.H \
	..\..\..\..\..\..\..\dev\tools\c32\inc\PSHPACK1.H \
	..\..\..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\pshpack8.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\rpc.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\rpcndr.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\rpcnsip.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\stdarg.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\stdlib.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\unknwn.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\winerror.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\winperf.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\winsock.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\winsvc.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\winver.h \
	..\..\..\..\..\..\..\dev\tools\c32\inc\wtypes.h ..\..\..\ioctl.h
.PRECIOUS: $(OBJDIR)\config.lst
