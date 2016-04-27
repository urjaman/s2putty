/*    dialer.h
 *
 * A network connection setup class
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2004 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __DIALER_H__
#define __DIALER_H__

#include <AgentClient.h>
#include <nifman.h>

class CIntConnectionInitiator;


/**
 * An observer class for CDialer. The observer gets notified when the
 * connection has been set up, or there has been an error.
 */
class MDialObserver {
public:
    /** 
     * Notifies the observer that a connection setup has completed.
     * 
     * @param aError The error code, KErrNone if the connection was set up
     *               successfully.
     */
    virtual void DialCompletedL(TInt aError) = 0;
};


/**
 * A network connection setup class. Connects to the network asynchronously
 * and notifies an observer when the connection has been set up.
 */
class CDialer : public CActive {
    
public:

    /** 
     * Constructs a new CDialer object.
     * 
     * @param aObserver The observer to use
     * 
     * @return A new CDialer object
     */
    static CDialer *NewL(MDialObserver *aObserver);
    
    static CDialer *NewLC(MDialObserver *aObserver);

    /** 
     * Destructor.
     */
    ~CDialer();
    
    /** 
     * Starts setting up a network connection. When the connection has
     * completed, or there has been an error, the observer is notified.
     */
    void DialL();

    /** 
     * Cancels the connection setup.
     */
    void CancelDialL();

    /**
     * Closes the connection if one was actively started by this CDialer
     * object. If an existing connection was used, this method does nothing.
     */
    void CloseConnectionL();
    
protected:
    // CActive methods
    void DoCancel();
    void RunL();

    void DoDialL();

private:
    CDialer(MDialObserver *aObserver);
    void ConstructL();

    enum {
        EStateNone = 0,
        EStateDialing,
        EStateConnected,
        EStateOldConnection,
        EStateError
    } iState;

    MDialObserver *iObserver;

    RNif iNif;
    TBool iNifOpen;
    TBool iNifTimersDisabled;

    CIntConnectionInitiator *iIntConnInit;
    TBool iIntConnInitBroken;

    TInt iAccessDeniedRetryCount;
};


#endif
