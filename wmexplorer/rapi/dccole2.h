//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    dccole2.h

Abstract:

    This file defines OLE interface to the DCCMAN module (Desktop Only). 
    It includes the old interfaces from dccole.h and contains 
    a new IDCCManSink2 interface for IPv6 support.
    
Environment:

    User Mode - Win32

--*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// {A7B88840-A812-11cf-8011-00A0C90A8F78}
DEFINE_GUID(IID_IDccManSink, 
0xa7b88840, 0xa812, 0x11cf, 0x80, 0x11, 0x0, 0xa0, 0xc9, 0xa, 0x8f, 0x78);
// {A7B88841-A812-11cf-8011-00A0C90A8F78}
DEFINE_GUID(IID_IDccMan, 
0xa7b88841, 0xa812, 0x11cf, 0x80, 0x11, 0x0, 0xa0, 0xc9, 0xa, 0x8f, 0x78);
// {499C0C20-A766-11cf-8011-00A0C90A8F78}
DEFINE_GUID(CLSID_DccMan, 
0x499c0c20, 0xa766, 0x11cf, 0x80, 0x11, 0x0, 0xa0, 0xc9, 0xa, 0x8f, 0x78);

// {1317003A-9A62-4040-98F1-9CE9EFD8298B}
DEFINE_GUID(IID_IDccManSink2, 
0x1317003a, 0x9a62, 0x4040, 0x98, 0xf1, 0x9c, 0xe9, 0xef, 0xd8, 0x29, 0x8b);

#ifndef _DCCOLEH_
#define _DCCOLEH_

#undef  INTERFACE
#define INTERFACE   IDccManSink

DECLARE_INTERFACE_ (IDccManSink,  IUnknown)
{
 	// These methods corespond to GW_LOG messages generated by the Win95 DCC
    STDMETHOD(OnLogIpAddr) (THIS_ DWORD dwIpAddr) PURE;
    STDMETHOD(OnLogTerminated) (THIS) PURE;
    STDMETHOD(OnLogActive) (THIS) PURE;
	STDMETHOD(OnLogInactive) (THIS) PURE;
	STDMETHOD(OnLogAnswered) (THIS) PURE;
	STDMETHOD(OnLogListen) (THIS) PURE;
	STDMETHOD(OnLogDisconnection) (THIS) PURE;
	STDMETHOD(OnLogError) (THIS) PURE;
};
typedef IDccManSink *LPDCCMANSINK;

#undef  INTERFACE
#define INTERFACE   IDccMan

DECLARE_INTERFACE_ (IDccMan,  IUnknown)
{
    STDMETHOD(Advise) (THIS_
		IN	IDccManSink * pDccSink,	// The advise sink that is requesting notification
		OUT DWORD * pdwContext		// Identifies the context for future calls to the Unadvise method
	) PURE;
	
	STDMETHOD(Unadvise) (THIS_
		DWORD dwContext				// As returned by Advise()
	) PURE;
   
	STDMETHOD(ShowCommSettings) (THIS) PURE;	// Displays the Communication Property Sheet on the screen
												// If a connection is active, the sheet is in read-only mode
	STDMETHOD(AutoconnectEnable) (THIS) PURE;
	STDMETHOD(AutoconnectDisable) (THIS) PURE;

	STDMETHOD(ConnectNow) (THIS) PURE;			// Active only when Autoconnect is Disabled
	STDMETHOD(DisconnectNow) (THIS) PURE;		// Active only when Autoconnect is Disabled
	
	STDMETHOD(SetIconDataTransferring) (THIS) PURE;
	STDMETHOD(SetIconNoDataTransferring) (THIS) PURE;
	STDMETHOD(SetIconError) (THIS) PURE;
};
typedef IDccMan *LPDCCMAN;

#endif /* end, ifdef _DCCOLEH_  */

//================ New Sink Interface for IPv6 support ========================
#ifndef _DCCOLE2H_
#define _DCCOLE2H_

#include <winsock2.h>
#undef  INTERFACE
#define INTERFACE   IDccManSink2

DECLARE_INTERFACE_ (IDccManSink2,  IDccManSink)
{
    STDMETHOD(OnLogIpAddrEx) (THIS_ const SOCKADDR_STORAGE* pIpAddr) PURE;
};
typedef IDccManSink2 *LPDCCMANSINK2;

#endif /*_DCCOLE2H_*/


#ifdef __cplusplus
}
#endif  /* __cplusplus */

