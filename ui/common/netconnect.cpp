/*    netconnect.cpp
 *
 * A concrete network connection setup class using RConnection
 *
 * Copyright 2006 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <es_sock.h>
#ifdef PUTTY_S60V3
    #include <commdbconnpref.h>
    #include <commdb.h>
#endif

#ifdef PUTTY_S60V2
    #include <commdbconnpref.h>
    #include <commdb.h>
#endif

#ifdef PUTTY_SYM3
    #include <extendedconnpref.h>
#endif

#include "netconnect.h"

_LIT(KPanic, "netconnect");
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KPanic, __LINE__))


// Factory
CNetConnect *CNetConnect::NewL(MNetConnectObserver &aObserver) {
    CNetConnect *self = new (ELeave) CNetConnect(aObserver);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}


// Constructor
CNetConnect::CNetConnect(MNetConnectObserver &aObserver)
    : CActive(EPriorityStandard),
      iObserver(aObserver) {
    CActiveScheduler::Add(this);
}


// Second-phase constructor
void CNetConnect::ConstructL() {
    iPromptAP = 0;
}


// Destructor
CNetConnect::~CNetConnect() {
    if ( iRConnectionOpen ) {
        iConnection.Close();
    }
    if ( iSocketServOpen ) {
        iSocketServ.Close();
    }
}


// Connect
void CNetConnect::Connect() {

    assert(iState == EStateNone);

    if ( !iSocketServOpen ) {
        TInt err = iSocketServ.Connect();
        if ( err != KErrNone ) {
            iObserver.NetConnectComplete(err, iSocketServ, iConnection);
            return;
        }
        iSocketServOpen = ETrue;
    }
    
    // Open RConnection
    TInt err = iConnection.Open(iSocketServ);
    if ( err != KErrNone ) {
        iObserver.NetConnectComplete(err, iSocketServ, iConnection);
        return;
    }
    iRConnectionOpen = ETrue;
#ifdef PUTTY_SYM3
    //seems in sym^3 connection methodes have changed.
    // more info from 
    // http://developer.symbian.org/wiki/One_Click_Connectivity_%28OCC%29_-_A_guide_for_developers#Disallow_forced_reconnection
    // http://developer.symbian.org/wiki/IP_connection_management_code_examples#Opening_a_connection_and_disabling_forced_reconnection
    if ( iPromptAP == 0 ) {
        // Create connection preferences
        TConnPrefList prefList;
        TExtendedConnPref prefs;
        prefs.SetConnSelectionDialog( ETrue );
        prefs.SetForcedRoaming(EFalse); // disable ALR
        prefList.AppendL( &prefs );
        iConnection.Start(prefList, iStatus);
    } else if ( iPromptAP == 1 ) {
        // Connect to the network Default Internet AP
        TConnPrefList prefList;
        TExtendedConnPref prefs;
        prefs.SetSnapPurpose( CMManager::ESnapPurposeInternet );
        prefs.SetNoteBehaviour( TExtendedConnPref::ENoteBehaviourConnSilent );
        prefs.SetForcedRoaming( EFalse ); // disable ALR
        prefList.AppendL( &prefs );
        iConnection.Start(prefList, iStatus);
    } else {        
        //Connect without prompting to user set AP
        TConnPrefList prefList;
        TExtendedConnPref prefs;
        prefs.SetNoteBehaviour( TExtendedConnPref::ENoteBehaviourConnSilent ); // should not be needed.
        prefs.SetConnSelectionDialog( EFalse ); // dont prompt
        prefs.SetIapId(ConvertPromptApToAPIdL(iPromptAP-2)); // set access point to connect
        prefs.SetForcedRoaming( EFalse ); // disable ALR, altough this should already be false in this case.
        prefList.AppendL( &prefs );        
        iConnection.Start(prefList, iStatus);            
    }    
#else
#ifdef PUTTY_S60V2
    if (( iPromptAP == 0 ) || (iPromptAP == 1 )) {
#else
    if ( iPromptAP == 0 ) {
        TCommDbConnPref pref;
        //pref.SetIapId(iIapId); // set access point to connect
        //ECommDbDialogPrefDoNotPrompt == do not prompt for ap
        pref.SetDialogPreference( ECommDbDialogPrefPrompt  );
        iConnection.Start(pref, iStatus);
    } else if ( iPromptAP == 1 ) {
#endif
        // Connect to the network Default Internet AP
        iConnection.Start(iStatus);
    } else {        
        //Connect without prompting to user set AP
        TCommDbConnPref pref;
        pref.SetIapId(ConvertPromptApToAPIdL(iPromptAP-2)); // set access point to connect           
        pref.SetDialogPreference( ECommDbDialogPrefDoNotPrompt  );
        iConnection.Start(pref, iStatus);            
    }
#endif
    iState = EStateConnecting;
    SetActive();
}


// Cancel connection
void CNetConnect::CancelConnect() {
    if ( iState != EStateConnecting ) {
        return;
    }
    Cancel();
}


// Active object RunL
void CNetConnect::RunL() {
    if ( iStatus == KErrNone ) {
        iState = EStateConnected;
    }
    iObserver.NetConnectComplete(iStatus.Int(), iSocketServ, iConnection);
}


// Active object cancel
void CNetConnect::DoCancel() {
    assert(iState == EStateConnecting);
    iConnection.Close();
    iRConnectionOpen = EFalse;
    iState = EStateNone;
}

// Looks for the access point ID
TUint32 CNetConnect::ConvertPromptApToAPIdL(TInt aValue) {
    TUint32 iapID = 0;
    CCommsDatabase* iCommsDB=CCommsDatabase::NewL();
    TInt i = 0;
    TInt err = KErrNone;
    CleanupStack::PushL(iCommsDB);

#ifdef PUTTY_S60V3  
    CCommsDbTableView* iIAPView = iCommsDB->OpenIAPTableViewMatchingBearerSetLC(
            ECommDbBearerGPRS|ECommDbBearerWLAN|ECommDbBearerVirtual,
            ECommDbConnectionDirectionOutgoing); 
#else  
    CCommsDbTableView* iIAPView = iCommsDB->OpenTableLC((TPtrC(IAP)));
#endif

    if ( iIAPView->GotoFirstRecord() == KErrNone ){
        do
        {
            if ( i == aValue ) {
                iIAPView->ReadUintL(TPtrC(COMMDB_ID), iapID);            
                break; // break from loop
            }
            i++;
        } while ( err = iIAPView->GotoNextRecord(), err == KErrNone);
    }

    CleanupStack::PopAndDestroy(); // view
    CleanupStack::PopAndDestroy(); // commDB
    return iapID;
}

