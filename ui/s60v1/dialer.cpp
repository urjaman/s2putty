/*    dialer.cpp
 *
 * A network connection setup class
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2004 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <intconninit.h>
#include "dialer.h"

_LIT(KAssertPanic, "dialer.cpp");
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KAssertPanic, __LINE__))
_LIT(KDialObserver, "DialObserver");
_LIT(KDialerPanic2, "dialer2");

const TInt KMaxAccessDeniedRetryCount = 3;


CDialer *CDialer::NewL(MDialObserver *aObserver) {
    CDialer *self = NewLC(aObserver);;
    CleanupStack::Pop(self);
    return self;
}

CDialer* CDialer::NewLC(MDialObserver* aObserver) {
    CDialer* self = new (ELeave) CDialer(aObserver);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}


CDialer::CDialer(MDialObserver *aObserver)
    : CActive(EPriorityNormal),
      iObserver(aObserver) {
}


CDialer::~CDialer() {
    Cancel();
    if ( iNifTimersDisabled ) {
        iNif.DisableTimers(EFalse);
    }
    if ( iNifOpen ) {
        iNif.Close();
    }    
    delete iIntConnInit;
}


void CDialer::ConstructL() {

    // On a 6600, CIntConnectionInitiator::NewL() will fail with
    // KErrNotFound once after an access point has been deleted and a
    // new one created, at least if the access point was the only one
    // in use. After a single network connection has been established
    // automatically, future connection attempts will work fine. To
    // work around this, we'll simply return immediately from
    // connection attempts if this breakage happends.
    //
    // On Series 60 2.0, RConnection is the preferred way to set up
    // connections. It's not available in previous versions though, and we
    // don't want to build separate binaries for different versions...
    TRAPD(err, iIntConnInit = CIntConnectionInitiator::NewL());
    if ( err == KErrNotFound ) {
        iIntConnInitBroken = ETrue;
    } else {
        User::LeaveIfError(err);
        User::LeaveIfError(iNif.Open());
        iNifOpen = ETrue;
    }

    CActiveScheduler::Add(this);
}


void CDialer::DialL() {
    assert(iState == EStateNone);

    // Disable connection inactivity timers
    
    // Work around a broken CIntConnectionInitiator
    if ( iIntConnInitBroken ) {
        iObserver->DialCompletedL(KErrNone);
        iState = EStateOldConnection;
        return;
    }

    // Note that we don't check for the return code. That's because the call
    // fails with KErrNotReady on a 6600, but it still seems to work. We can't
    // defer the call to after the connection has been set up, since in that
    // case the connection will get closed automatically before the SSH socket
    // gets opened. Sigh.
    iNif.DisableTimers(ETrue);
    iNifTimersDisabled = ETrue;
            
    iAccessDeniedRetryCount = 0;
    DoDialL();
} 


void CDialer::DoDialL() {
    // Build connection preferences
    CCommsDbConnectionPrefTableView::TCommDbIapConnectionPref pref;
    pref.iRanking = 1;
    pref.iDirection = ECommDbConnectionDirectionOutgoing;
    pref.iDialogPref = ECommDbDialogPrefPrompt;
    pref.iBearer.iBearerSet = ECommDbBearerUnknown;
    pref.iBearer.iIapId = 0; // undefined IAP

    // Start connection
    // Note that at CIntConnectionInitiator in least Series 60 v1.2 won't set
    // the status object to KRequestPending by itself, so we'll need to do that
    // before calling ConnectL(), otherwise we'll eat events from other active
    // objects. v2.0 does work without this, but it doesn't hurt...
    iStatus = KRequestPending;
    iIntConnInit->ConnectL(pref, iStatus);
    SetActive();
    iState = EStateDialing;
}


void CDialer::RunL() {
    TInt err = iStatus.Int();

    switch ( err ) {
        case KConnectionCreated:
        case KConnectionExists:
        case KConnectionPref1Created:
        case KConnectionPref1Exists:
        case KConnectionPref2Created:
        case KConnectionPref2Exists:
            iState = EStateConnected;
            err = KErrNone;
            break;

        case KErrAccessDenied: {
            // According to the Forum Nokia IAPConnect example, changing the
            // IAP may fail with KErrAccessDenied on the first try. To work
            // around this, we'll retry a couple of times.
            iState = EStateNone;
            iAccessDeniedRetryCount++;
            if ( iAccessDeniedRetryCount < KMaxAccessDeniedRetryCount ) {
                TRAP(err, DoDialL());
                if ( err == KErrNone )
                    return;
            }
            break;
        }

        default: {
            // According to the IAPConnect example,
            // CIntConnectionInitiator needs to be destroyed and recreated if
            // errors occur and the user might want to retry
            iState = EStateNone;
            iIntConnInit->Cancel();
            delete iIntConnInit;
            TRAPD(err2, iIntConnInit = CIntConnectionInitiator::NewL());
            if ( err2 != KErrNone ) {
                User::Panic(KDialerPanic2, err2);
            }
            break;
        }
    }
    
    TRAPD(error, iObserver->DialCompletedL(err));
    if ( error != KErrNone ) {
        User::Panic(KDialObserver, error);
    }
}


void CDialer::DoCancel() {
    assert(iState == EStateDialing);
    // Not sure if this is really correct, but this is what the IAPConnect
    // example does...
    iIntConnInit->Cancel();
}


void CDialer::CancelDialL() {

    if ( iState == EStateDialing ) {
        Cancel();
        if ( iNifTimersDisabled ) {
            User::LeaveIfError(iNif.DisableTimers(EFalse));
            iNifTimersDisabled = EFalse;
        }
        iState = EStateNone;
    } else if ( iState == EStateConnected ) {
        User::LeaveIfError(iIntConnInit->TerminateActiveConnection());
        if ( iNifTimersDisabled ) {
            User::LeaveIfError(iNif.DisableTimers(EFalse));
            iNifTimersDisabled = EFalse;
        }
        iState = EStateNone;
    }
}


void CDialer::CloseConnectionL() {

    if ( iState == EStateDialing ) {
        CancelDialL();
    } else if ( iState == EStateConnected ) {
        User::LeaveIfError(iIntConnInit->TerminateActiveConnection());
        if ( iNifTimersDisabled ) {
            User::LeaveIfError(iNif.DisableTimers(EFalse));
            iNifTimersDisabled = EFalse;
        }
        iState = EStateNone;
    }
}
