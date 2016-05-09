/*    netconnect.h
 *
 * Network connection setup class
 *
 * Copyright 2006 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __NETCONNECT_H__
#define __NETCONNECT_H__

#include <e32base.h>
#include <es_sock.h>

#ifdef PUTTY_S60V1
#include <nifman.h>
class CIntConnectionInitiator;
#endif

/** 
 * Network connection observer class. The observer gets notified when the
 * connection has been set up, or there has been an error.
 * 
 * @param anError 
 */
class MNetConnectObserver {
public:
    /** 
     * Notifies the observer that network connection setup has completed.
     * 
     * @param aError Error code, KErrNone if the connection was set up
     * @param aSocketServ The socket server session for the connection
     * @param aConnection The network connection
     */
    virtual void NetConnectComplete(TInt aError,
                                    RSocketServ &aSocketServ
#ifndef PUTTY_S60V1
                                    ,RConnection &aConnection
#endif
) = 0;
};


/**
 * Network connection setup class. Connects to the network
 * asynchronously and notifies and observer when the connection has
 * been set up. The network connection will remain up as long as
 * this instance exists or there are open sockets.
 */
class CNetConnect : public CActive {
public:
    /** 
     * Factory method, constructs a new CNetConnect object
     * 
     * @param aObserver The observer object to use
     * 
     * @return A new CNetConnect object
     */
    static CNetConnect *NewL(MNetConnectObserver &aObserver);

    /** 
     * Connect to a network, asynchronously. Calls
     * MNetCOnnectObserver::NetConnectComplete() when the operation completes.
     */
    void Connect();

    //if aValue is set to 1 Prompt is valid else uses the default internet ap
    void SetPromptAP(TInt aValue ) { iPromptAP = aValue; };
    TUint32 ConvertPromptApToAPIdL(TInt aValue);
    
    /** 
     * Cancels a connection request.
     */
    void CancelConnect();

    /** 
     * Destructor
     */
    ~CNetConnect();

protected:
    // Constructor
    CNetConnect(MNetConnectObserver &aObserver);
    void ConstructL();
    
    // CActive methods
    void RunL();
    void DoCancel();

    // Data
    TInt iPromptAP;
    
    MNetConnectObserver &iObserver;
    RSocketServ iSocketServ;
    TBool iSocketServOpen;
#ifndef PUTTY_S60V1
    RConnection iConnection;    
    TBool iRConnectionOpen;
    enum {
        EStateNone,
        EStateConnecting,
        EStateConnected
    } iState;    
#else
    void DoConnect();
    RNif iNif;
    TBool iNifOpen;
    TBool iNifTimersDisabled;
    CIntConnectionInitiator *iIntConnInit;
    TBool iIntConnInitBroken;
    TInt iAccessDeniedRetryCount;
    enum {
        EStateNone = 0,
        EStateConnecting,
        EStateConnected,
        EStateOldConnection,
        EStateError
    } iState;
#endif

};


#endif
