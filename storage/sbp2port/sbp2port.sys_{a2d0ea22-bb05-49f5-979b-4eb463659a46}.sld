<?xml version="1.0" encoding="UTF-16"?>
<!DOCTYPE DCARRIER SYSTEM "mantis.dtd">
<DCARRIER CarrierRevision="1">
	<TOOLINFO ToolName="iCat"><![CDATA[<?xml version="1.0" encoding="UTF-16"?>
<!DOCTYPE TOOL SYSTEM "tool.dtd">
<TOOL>
	<CREATED><NAME>iCat</NAME><VSGUID>{97b86ee0-259c-479f-bc46-6cea7ef4be4d}</VSGUID><VERSION>1.0.0.452</VERSION><BUILD>452</BUILD><DATE>7/16/2001</DATE></CREATED><LASTSAVED><NAME>iCat</NAME><VSGUID>{97b86ee0-259c-479f-bc46-6cea7ef4be4d}</VSGUID><VERSION>1.0.0.452</VERSION><BUILD>452</BUILD><DATE>7/17/2001</DATE></LASTSAVED></TOOL>
]]></TOOLINFO><COMPONENT Revision="2" Visibility="1000" MultiInstance="0" Released="1" Editable="1" HTMLFinal="0" ComponentVSGUID="{A2D0EA22-BB05-49F5-979B-4EB463659A46}" ComponentVIGUID="{B8F4AFCD-B936-4AA2-9D06-F668963452BC}" PlatformGUID="{B784E719-C196-4DDB-B358-D9254426C38D}" RepositoryVSGUID="{8E0BE9ED-7649-47F3-810B-232D36C430B4}"><DISPLAYNAME>SBP2PORT.SYS</DISPLAYNAME><VERSION>1.0</VERSION><DESCRIPTION>SBP-2 Protocol Driver</DESCRIPTION><COPYRIGHT>2000 Microsoft Corp.</COPYRIGHT><VENDOR>Microsoft Corp.</VENDOR><OWNERS>dankn</OWNERS><AUTHORS>dankn</AUTHORS><DATECREATED>7/16/2001</DATECREATED><DATEREVISED>7/17/2001</DATEREVISED><RESOURCE ResTypeVSGUID="{E66B49F6-4A35-4246-87E8-5C1A468315B5}" BuildTypeMask="819" Name="File(819):&quot;%17%&quot;,&quot;sbp2.inf&quot;"><PROPERTY Name="DstPath" Format="String">%17%</PROPERTY><PROPERTY Name="DstName" Format="String">sbp2.inf</PROPERTY><PROPERTY Name="NoExpand" Format="Boolean">0</PROPERTY><DISPLAYNAME>SBP2.INF</DISPLAYNAME><DESCRIPTION>Setup file for SBP2PORT.SYS</DESCRIPTION></RESOURCE><RESOURCE ResTypeVSGUID="{E66B49F6-4A35-4246-87E8-5C1A468315B5}" BuildTypeMask="819" Name="File(819):&quot;%12%&quot;,&quot;sbp2port.sys&quot;"><PROPERTY Name="DstPath" Format="String">%12%</PROPERTY><PROPERTY Name="DstName" Format="String">sbp2port.sys</PROPERTY><PROPERTY Name="NoExpand" Format="Boolean">0</PROPERTY><DISPLAYNAME>SBP2PORT.SYS</DISPLAYNAME><DESCRIPTION>SBP-2 Protocol Driver</DESCRIPTION></RESOURCE><RESOURCE ResTypeVSGUID="{90D8E195-E710-4AF6-B667-B1805FFC9B8F}" BuildTypeMask="819" Name="RawDep(819):&quot;File&quot;,&quot;NTOSKRNL.EXE&quot;"><PROPERTY Name="RawType" Format="String">File</PROPERTY><PROPERTY Name="Value" Format="String">NTOSKRNL.EXE</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{90D8E195-E710-4AF6-B667-B1805FFC9B8F}" BuildTypeMask="819" Name="RawDep(819):&quot;File&quot;,&quot;HAL.DLL&quot;"><PROPERTY Name="RawType" Format="String">File</PROPERTY><PROPERTY Name="Value" Format="String">HAL.DLL</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{2C10DB69-39AB-48A4-A83F-9AB3ACBA7C45}" BuildTypeMask="819" Name="RegKey(819):&quot;HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port&quot;"><PROPERTY Name="KeyPath" Format="String">HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port</PROPERTY><PROPERTY Name="RegValue" Format="Binary"/><PROPERTY Name="RegType" Format="Integer">1</PROPERTY><PROPERTY Name="RegOp" Format="Integer">1</PROPERTY><PROPERTY Name="RegCond" Format="Integer">1</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{2C10DB69-39AB-48A4-A83F-9AB3ACBA7C45}" BuildTypeMask="819" Name="RegKey(819):&quot;HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port&quot;,&quot;Type&quot;"><PROPERTY Name="KeyPath" Format="String">HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port</PROPERTY><PROPERTY Name="ValueName" Format="String">Type</PROPERTY><PROPERTY Name="RegValue" Format="Integer">1</PROPERTY><PROPERTY Name="RegType" Format="Integer">4</PROPERTY><PROPERTY Name="RegOp" Format="Integer">1</PROPERTY><PROPERTY Name="RegCond" Format="Integer">1</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{2C10DB69-39AB-48A4-A83F-9AB3ACBA7C45}" BuildTypeMask="819" Name="RegKey(819):&quot;HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port&quot;,&quot;Start&quot;"><PROPERTY Name="KeyPath" Format="String">HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port</PROPERTY><PROPERTY Name="ValueName" Format="String">Start</PROPERTY><PROPERTY Name="RegValue" Format="Integer">0</PROPERTY><PROPERTY Name="RegType" Format="Integer">4</PROPERTY><PROPERTY Name="RegOp" Format="Integer">1</PROPERTY><PROPERTY Name="RegCond" Format="Integer">1</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{2C10DB69-39AB-48A4-A83F-9AB3ACBA7C45}" BuildTypeMask="819" Name="RegKey(819):&quot;HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port&quot;,&quot;ErrorControl&quot;"><PROPERTY Name="KeyPath" Format="String">HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port</PROPERTY><PROPERTY Name="ValueName" Format="String">ErrorControl</PROPERTY><PROPERTY Name="RegValue" Format="Integer">1</PROPERTY><PROPERTY Name="RegType" Format="Integer">4</PROPERTY><PROPERTY Name="RegOp" Format="Integer">1</PROPERTY><PROPERTY Name="RegCond" Format="Integer">1</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{2C10DB69-39AB-48A4-A83F-9AB3ACBA7C45}" BuildTypeMask="819" Name="RegKey(819):&quot;HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port&quot;,&quot;DisplayName&quot;"><PROPERTY Name="KeyPath" Format="String">HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port</PROPERTY><PROPERTY Name="ValueName" Format="String">DisplayName</PROPERTY><PROPERTY Name="RegValue" Format="String">SBP-2 Transport/Protocol Bus Driver</PROPERTY><PROPERTY Name="RegType" Format="Integer">1</PROPERTY><PROPERTY Name="RegOp" Format="Integer">1</PROPERTY><PROPERTY Name="RegCond" Format="Integer">1</PROPERTY></RESOURCE><RESOURCE ResTypeVSGUID="{2C10DB69-39AB-48A4-A83F-9AB3ACBA7C45}" BuildTypeMask="819" Name="RegKey(819):&quot;HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port&quot;,&quot;ImagePath&quot;"><PROPERTY Name="KeyPath" Format="String">HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\sbp2port</PROPERTY><PROPERTY Name="ValueName" Format="String">ImagePath</PROPERTY><PROPERTY Name="RegValue" Format="String">System32\DRIVERS\sbp2port.sys</PROPERTY><PROPERTY Name="RegType" Format="Integer">2</PROPERTY><PROPERTY Name="RegOp" Format="Integer">1</PROPERTY><PROPERTY Name="RegCond" Format="Integer">1</PROPERTY></RESOURCE><GROUPMEMBER GroupVSGUID="{E01B4103-3883-4FE8-992F-10566E7B796C}"/><GROUPMEMBER GroupVSGUID="{DE57766B-9566-11D4-8E84-00B0D03D27C6}"/><GROUPMEMBER GroupVSGUID="{DE577669-9566-11D4-8E84-00B0D03D27C6}"/><GROUPMEMBER GroupVSGUID="{DE577686-9566-11D4-8E84-00B0D03D27C6}"/><GROUPMEMBER GroupVSGUID="{DE57768C-9566-11D4-8E84-00B0D03D27C6}"/><GROUPMEMBER GroupVSGUID="{DE57767F-9566-11D4-8E84-00B0D03D27C6}"/><GROUPMEMBER GroupVSGUID="{DE57767C-9566-11D4-8E84-00B0D03D27C6}"/><GROUPMEMBER GroupVSGUID="{DE577687-9566-11D4-8E84-00B0D03D27C6}"/><DEPENDENCY Class="Include" Type="AtLeastOne" DependOnGUID="{7DBEA129-A893-494E-AB59-20C68318EE1D}"/></COMPONENT></DCARRIER>
