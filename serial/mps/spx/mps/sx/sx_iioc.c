/************************************************************************/
/*									*/
/*	Title		:	SX support functions for INTERNAL IOCTLs*/
/*									*/
/*	Author		:	N.P.Vassallo				*/
/*									*/
/*	Creation	:	14th October 1998			*/
/*									*/
/*	Version		:	1.0.0					*/
/*									*/
/*	Description	:	Support functions to support the	*/
/*				INTERNAL IOCTLs for specific hardware:	*/
/*					XXX_SetHandFlow			*/
/*									*/
/************************************************************************/

/* History...

1.0.0	14/20/98 NPV	Creation.

*/

#include "precomp.h"

/*****************************************************************************
*****************************                     ****************************
*****************************   XXX_SetHandFlow   ****************************
*****************************                     ****************************
******************************************************************************

prototype:	void	XXX_SetHandFlow(IN PPORT_DEVICE_EXTENSION pPort,IN SERIAL_IOCTL_SYNC *pS)

description:	Call to set the handshaking and flow control

parameters:	pPort points to the port device extension structure
		pS points to a serial ioctl synchronization structure

returns:	STATUS_SUCCESS

*/

void	XXX_SetHandFlow(IN PPORT_DEVICE_EXTENSION pPort,IN PSERIAL_IOCTL_SYNC pS)
{
	Slxos_SyncExec(pPort,SerialSetHandFlow,pS,0x0E);

} /* XXX_SetHandFlow */
                                                        
/* End of SX_IIOC.C */
