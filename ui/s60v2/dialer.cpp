/*    dialer.cpp
 *
 * A dial-up connection setup class
 *
 * Copyright 2002-2004 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include "dialer.h"

_LIT(KAssertPanic, "dialer.cpp");
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KAssertPanic, __LINE__))


// FIXME: Need an implementation for S60 2.0 using RConnection

CDialer *CDialer::NewL(MDialObserver *aObserver) {
    
    CDialer *self = new (ELeave) CDialer(aObserver);;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}


CDialer::CDialer(MDialObserver *aObserver) {
    
    iObserver = aObserver;
}


CDialer::~CDialer() {
}


void CDialer::ConstructL() {
}


void CDialer::DialL() {
    
    iObserver->DialCompletedL(KErrNone);
}


void CDialer::CancelDialL() {
}


void CDialer::CloseConnectionL() {
}
