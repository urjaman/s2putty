/*    profileeditdisplaysettinglist.cpp
 *
 * Putty profile edit view display setting list
 *
 * Copyright 2007 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <barsread.h>
#include <putty.rsg>
#include <akncommondialogs.h>
#include <badesca.h>
#include "profileeditdisplaysettinglist.h"
#include "profileeditview.h"
#include "dynamicenumtextsettingitem.h"
#include "puttyengine.h"
#include "stringutils.h"
#include "puttyuids.hrh"
#include "puttyui.hrh"

_LIT(KDefaultFont, "fixed6x13");
_LIT(KDefaultCharSet, "ISO-8859-15");
static const TInt KFullScreenWidth = 0xf5;


// Factory
CProfileEditDisplaySettingList *CProfileEditDisplaySettingList::NewL(
    CPuttyEngine &aPutty, CProfileEditView &aView, const CDesCArray &aFonts) {
    CProfileEditDisplaySettingList *self =
        new (ELeave) CProfileEditDisplaySettingList(aPutty, aView, aFonts);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}


// Constructor
CProfileEditDisplaySettingList::CProfileEditDisplaySettingList(
    CPuttyEngine &aPutty, CProfileEditView &aView, const CDesCArray &aFonts)
    : CProfileEditSettingListBase(aPutty, aView),
      iFonts(aFonts) {
}


// Second-phase constructor
void CProfileEditDisplaySettingList::ConstructL() {
    iConfig = iPutty.GetConfig();
    ConstructFromResourceL(R_PUTTY_PROFILEEDIT_DISPLAY_SETTINGLIST);
    ActivateL();
}


// Destructor
CProfileEditDisplaySettingList::~CProfileEditDisplaySettingList() {
    delete iCharSets;
}


// CAknSettingItemList::CreateSettingItemL()
CAknSettingItem *CProfileEditDisplaySettingList::CreateSettingItemL(
    TInt aIdentifier) {

    switch ( aIdentifier ) {
        case EPuttySettingDisplayFont: {
            // Find the current font in the list
            HBufC *cur = StringToBufLC(iConfig->font.name);
            if ( iFonts.Find(*cur, iFontValue) != 0 ) {
                // Not found, use default
                if ( iFonts.Find(KDefaultFont, iFontValue) != 0 ) {
                    iFontValue = 0;
                }
            }
            CleanupStack::PopAndDestroy(); //cur
            
            return new (ELeave) CDynamicEnumTextSettingItem(
                aIdentifier, iFonts, iFontValue);
        }
            
        case EPuttySettingDisplayFullScreen:
            iFullScreen = (iConfig->width == KFullScreenWidth);
            return new (ELeave) CAknBinaryPopupSettingItem(aIdentifier, 
                                                           iFullScreen);

        case EPuttySettingDisplayCharSet: {
            // Get character sets from the engine
            iCharSets = iPutty.SupportedCharacterSetsL();

            // Find the current character set in the list
            HBufC *cur = StringToBufLC(iConfig->line_codepage);
            if ( iCharSets->Find(*cur, iCharSetValue) != 0 ) {
                // Not found, use default
                if ( iCharSets->Find(KDefaultCharSet, iCharSetValue) != 0 ) {
                    iCharSetValue = 0;
                }
            }
            CleanupStack::PopAndDestroy(); //cur
            
            return new (ELeave) CDynamicEnumTextSettingItem(
                aIdentifier, *iCharSets, iCharSetValue);
        }
    }

    return NULL;
}


// CAknSettingItemList::EditItemL()
void CProfileEditDisplaySettingList::EditItemL(TInt aIndex,
                                               TBool aCalledFromMenu) {
    TInt id = (*SettingItemArray())[aIndex]->Identifier();

    CAknSettingItemList::EditItemL(aIndex, aCalledFromMenu);
    
    // Always store changes to the variable that the item uses
    (*SettingItemArray())[aIndex]->StoreL();

    // Store the change to PuTTY config if needed
    switch ( id ) {
        case EPuttySettingDisplayFont: {
            DesToString(iFonts[iFontValue], iConfig->font.name,
                        sizeof(iConfig->font.name));
            break;
        }

        case EPuttySettingDisplayFullScreen: {
            if ( iFullScreen ) {
                iConfig->width = KFullScreenWidth;
            } else {
                iConfig->width = 0;
            }
            break;
        }
            
        case EPuttySettingDisplayCharSet: {
            DesToString((*iCharSets)[iCharSetValue], iConfig->line_codepage,
                        sizeof(iConfig->line_codepage));
            break;
        }
            
        default:
            ;
    }
}
