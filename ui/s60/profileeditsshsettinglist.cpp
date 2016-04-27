/*    profileeditsshsettinglist.cpp
 *
 * Putty profile edit view SSH setting list
 *
 * Copyright 2007 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <barsread.h>
#include <putty.rsg>
#include <akncommondialogs.h>
#include "profileeditsshsettinglist.h"
#include "profileeditview.h"
#include "puttyengine.h"
#include "stringutils.h"
#include "puttyuids.hrh"
#include "puttyui.hrh"


// Factory
CProfileEditSshSettingList *CProfileEditSshSettingList::NewL(
    CPuttyEngine &aPutty, CProfileEditView &aView) {
    CProfileEditSshSettingList *self =
        new (ELeave) CProfileEditSshSettingList(aPutty, aView);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}


// Constructor
CProfileEditSshSettingList::CProfileEditSshSettingList(
    CPuttyEngine &aPutty, CProfileEditView &aView)
    : CProfileEditSettingListBase(aPutty, aView) {
}


// Second-phase constructor
void CProfileEditSshSettingList::ConstructL() {
    iConfig = iPutty.GetConfig();
    ConstructFromResourceL(R_PUTTY_PROFILEEDIT_SSH_SETTINGLIST);
    ActivateL();
}


// Destructor
CProfileEditSshSettingList::~CProfileEditSshSettingList() {
}


// CAknSettingItemList::CreateSettingItemL()
CAknSettingItem *CProfileEditSshSettingList::CreateSettingItemL(
    TInt aIdentifier) {

    switch ( aIdentifier ) {
        case EPuttySettingSshPort:
            return new (ELeave) CAknIntegerEdwinSettingItem(aIdentifier,
                                                            iConfig->port);

        case EPuttySettingSshVersion:
            return new (ELeave) CAknEnumeratedTextPopupSettingItem(
                aIdentifier, iConfig->sshprot);

        case EPuttySettingSshPrivateKey:
            StringToDes(iConfig->keyfile.path, iPrivateKey);
            return new (ELeave) CAknTextSettingItem(aIdentifier, iPrivateKey);
            
        case EPuttySettingSshCompression:
            iCompression = (iConfig->compression != 0);
            return new (ELeave) CAknBinaryPopupSettingItem(aIdentifier, 
                                                           iCompression);

        case EPuttySettingSshKeepalive:
            return new (ELeave) CAknIntegerEdwinSettingItem(
                aIdentifier, iConfig->ping_interval);
    }

    return NULL;
}


// CAknSettingItemList::EditItemL()
void CProfileEditSshSettingList::EditItemL(TInt aIndex,
                                           TBool aCalledFromMenu) {
    TInt id = (*SettingItemArray())[aIndex]->Identifier();

    if ( id == EPuttySettingSshPrivateKey ) {
        // Show file selection dialog for the private key instead of editing
        // the text
        if ( AknCommonDialogs::RunSelectDlgLD(iPrivateKey,
                                              R_PUTTY_MEMORY_SELECTION_DIALOG) ) {
            DesToString(iPrivateKey, iConfig->keyfile.path,
                        sizeof(iConfig->keyfile.path));
            (*SettingItemArray())[aIndex]->LoadL();
            (*SettingItemArray())[aIndex]->UpdateListBoxTextL();
        }
        return;
    };

    CAknSettingItemList::EditItemL(aIndex, aCalledFromMenu);
    
    // Always store changes to the variable that the item uses
    (*SettingItemArray())[aIndex]->StoreL();

    // Store the change to PuTTY config if needed
    switch ( id ) {
        case EPuttySettingSshCompression:
            iConfig->compression = (int) iCompression;
            break;
            
        default:
            ;
    }
}
