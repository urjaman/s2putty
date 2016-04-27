/*    dialer.h
 *
 * A dial-up connection setup class
 *
 * Copyright 2002 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __DIALER_H__
#define __DIALER_H__

#include <e32base.h>


/**
 * An observer class for CDialer. The observer gets notified when the
 * connection has been set up, or there has been an error.
 */
class MDialObserver {
public:
    /** 
     * Notifies the observer that a connection setup has completed.
     * 
     * @param anError The error code, KErrNone if the connection was set up
     *                successfully.
     */
    virtual void DialCompletedL(TInt anError) = 0;
};


/**
 * A dial-up connection setup class. Connects to the network asynchronously
 * and notifies an observer when the connection has been set up.
 */
class CDialer : public CBase {
    
public:
    /** 
     * Constructs a new CDialer object.
     * 
     * @param aObserver The observer to use
     * 
     * @return A new CDialer object
     */
    static CDialer *NewL(MDialObserver *aObserver);

    /** 
     * Destructor.
     */
    ~CDialer();
    
    /** 
     * Starts setting up a dialup connection. When the connection has
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

private:
    CDialer(MDialObserver *aObserver);
    void ConstructL();

    MDialObserver *iObserver;
};


#endif
