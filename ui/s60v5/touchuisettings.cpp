/*    touchuisettings.cpp
 *
 * Putty touch ui settings file
 *
 * Copyright 2009 Risto Avila
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <e32cmn.h>
#include <f32file.h>
#include <s32file.h>
#include <bautils.h>
#include <eikdef.h>
#include <aknview.h>
#include <aknviewappui.h>

#include "touchuisettings.h"
#include "customtoolbar.h"
#include "puttyui.hrh"

const TInt KTouchUiSettingsVersion = 8;

TTouchSettings::TTouchSettings() {
    
    SetDefault();
}

TTouchSettings::~TTouchSettings() {

}

TBool TTouchSettings::TestIfToolBarActionAllreadySet(TInt aCmd) {
    TBool ret = EFalse;
    
    if (itbButton1 == aCmd) {
        ret = ETrue;
    }
    if (itbButton2 == aCmd) {
        ret = ETrue;
    }
    if (itbButton3 == aCmd) {
        ret = ETrue;
    }
    if (itbButton4 == aCmd) {
        ret = ETrue;
    }
    if (itbButton5 == aCmd) {
        ret = ETrue;
    }
    if (itbButton6 == aCmd) {
        ret = ETrue;
    }
    if (itbButton7 == aCmd) {
        ret = ETrue;
    }
    if (itbButton8 == aCmd) {
        ret = ETrue;
    }
    return ret;
}

TBool TTouchSettings::SwapButtons(TInt aButton, TInt aCmd) {

    TInt *ButtonToSwap = NULL;
    TInt *ButtonHasCmd = NULL;
    switch (aButton) {
        case 0:
            ButtonToSwap = &itbButton1;
            break;
        case 1:
            ButtonToSwap = &itbButton2;
            break;
        case 2:
            ButtonToSwap = &itbButton3;
            break;
        case 3:
            ButtonToSwap = &itbButton4;
            break;
        case 4:
            ButtonToSwap = &itbButton5;
            break;
        case 5:
            ButtonToSwap = &itbButton6;
            break;
        case 6:
            ButtonToSwap = &itbButton7;
            break;
        case 7:
            ButtonToSwap = &itbButton8;
            break;
            
    }
    
    if (itbButton1 == aCmd) {
        ButtonHasCmd = &itbButton1;   
    } else if (itbButton2 == aCmd) {
        ButtonHasCmd = &itbButton2;   
    } else if (itbButton3 == aCmd) {
        ButtonHasCmd = &itbButton3;   
    } else if (itbButton4 == aCmd) {
        ButtonHasCmd = &itbButton4;   
    } else if (itbButton5 == aCmd) {
        ButtonHasCmd = &itbButton5;   
    } else if (itbButton6 == aCmd) {
        ButtonHasCmd = &itbButton6;   
    } else if (itbButton7 == aCmd) {
        ButtonHasCmd = &itbButton7;    
    } else if (itbButton8 == aCmd) {
        ButtonHasCmd = &itbButton8;    
    } else {
        return EFalse;
    }
    TInt tmp = *ButtonToSwap;
    *ButtonToSwap = *ButtonHasCmd;
    *ButtonHasCmd = tmp;
    return ETrue;
}

void TTouchSettings::SetDefault() {
    
    //todo change default actions to
    // Swipe right = alt + left ?
    // Swipe left = alt + right ?

    iShowtoolbar = 1; //1 show, 0 no show
    iAllowMouseGrab = 1;
    iSingleTap = EPuttyCmdStartVKB;
    iDoubleTap = EPuttyCmdSend;
    iLongTap = EPuttyCmdOpenPopUpMenu;
    iSwipeLeft = EPuttyCmdToggleToolbar;
    iSwipeRight = EPuttyCmdToggleToolbar;
    iSwipeUp = EPuttyCmdSendPageDown;
    iSwipeDown = EPuttyCmdSendPageUp;    
    /* old defaults
    itbButton1 = EPuttyToolbarTab;
    itbButton2 = EPuttyToolbarAltP;
    itbButton3 = EPuttyToolbarCtrlP;
    itbButton4 = EPuttyToolbarPipe;
    itbButton5 = EPuttyToolbarLock;
    itbButton6 = EPuttyToolbarSelect;
    itbButton7 = EPuttyToolbarCopy;
    itbButton8 = EPuttyToolbarPaste;
    */

    itbButton1 = EPuttyToolbarTab;
    itbButton2 = EPuttyToolbarAltP;
    itbButton3 = EPuttyToolbarCtrlP;
    itbButton4 = EPuttyToolbarPipe;
    itbButton5 = EPuttyToolbarLock;
    itbButton6 = EPuttyToolbarGrid; // send grid
    itbButton7 = EPuttyToolbarListWeb;
    itbButton8 = EPuttyToolbarEnter;
    
    itbButtonCount = 8;
    itbButtonWidth = 60;
    itbButtonHeigth = 60;
    
    //These are up values 
    iButtonUpBackgroundTransparency = 90; //old default 179
    iButtonUpTextTransparency = 250;
    iButtonDownBackgroundTransparency = 20; //old default 59
    iButtonDownTextTransparency = 148;
    iButtonFontSize = 0;
    
    iSaveLandscapeMovePoint.iX = 0;        
    iSaveLandscapeMovePoint.iY = 0;        
    iSaveLandscapeIsStartPlaceTop = ETrue; 
    
    iSavePortraitMovePoint.iX = 0;
    iSavePortraitMovePoint.iY = 0;
    iSavePortraitIsStartPlaceTop = ETrue;
}

void TTouchSettings::ReadSettingFileL(const TDesC &aFileName) {

    CEikonEnv *eikenv = CEikonEnv::Static();
    RFs &fs = eikenv->FsSession();
    
    RFileReadStream fRead;
    User::LeaveIfError( fRead.Open( fs, aFileName, EFileRead ) );
    CleanupClosePushL(fRead);

    // Check settings file version
    TInt iVersion = -1;
    TRAPD(error, iVersion = fRead.ReadInt16L()); // might fail if the file is empty
    if ( (error != KErrNone) || (iVersion != KTouchUiSettingsVersion) ) {
        // Reset to defaults and fail quietly if versions don't match
        CleanupStack::PopAndDestroy(&fRead);
        SetDefault();
        return;
    }
    TInt tmp = 0;
    
    iShowtoolbar = fRead.ReadInt16L();
    iAllowMouseGrab = fRead.ReadInt16L();
    iSingleTap = fRead.ReadInt16L();
    iDoubleTap = fRead.ReadInt16L();
    iLongTap = fRead.ReadInt16L();
    iSwipeLeft = fRead.ReadInt16L();
    iSwipeRight = fRead.ReadInt16L();
    iSwipeUp = fRead.ReadInt16L();
    iSwipeDown = fRead.ReadInt16L();
    
    itbButton1 = fRead.ReadInt16L();
    itbButton2 = fRead.ReadInt16L();
    itbButton3 = fRead.ReadInt16L();
    itbButton4 = fRead.ReadInt16L();
    itbButton5 = fRead.ReadInt16L();
    itbButton6 = fRead.ReadInt16L();
    itbButton7 = fRead.ReadInt16L();
    itbButton8 = fRead.ReadInt16L();
    
    itbButtonCount = fRead.ReadInt16L();
    itbButtonWidth = fRead.ReadInt16L();
    itbButtonHeigth = fRead.ReadInt16L();
    iButtonUpBackgroundTransparency = fRead.ReadInt16L();
    iButtonUpTextTransparency = fRead.ReadInt16L();
    iButtonDownBackgroundTransparency = fRead.ReadInt16L();
    iButtonDownTextTransparency = fRead.ReadInt16L();
    iButtonFontSize = fRead.ReadInt16L(); 

    iSaveLandscapeMovePoint.iX = fRead.ReadInt16L();        
    iSaveLandscapeMovePoint.iY = fRead.ReadInt16L();        
    tmp = fRead.ReadInt16L();
    if ( tmp == 1 ) {
        iSaveLandscapeIsStartPlaceTop = ETrue;
    } else {
        iSaveLandscapeIsStartPlaceTop = EFalse;
    }
    
    iSavePortraitMovePoint.iX = fRead.ReadInt16L();
    iSavePortraitMovePoint.iY = fRead.ReadInt16L();
    tmp = fRead.ReadInt16L();
    if ( tmp == 1 ) {
        iSavePortraitIsStartPlaceTop = ETrue;
    } else {
        iSavePortraitIsStartPlaceTop = EFalse;
    }
        
    CleanupStack::PopAndDestroy(&fRead);
}

void TTouchSettings::WriteSettingFileL(const TDesC &aFileName) {
    
    RFs &fs = CEikonEnv::Static()->FsSession();

    RFile myFile;
    RFileWriteStream fWrite;
    
    User::LeaveIfError( fWrite.Replace( fs, aFileName, EFileWrite ) );
    
    CleanupClosePushL(fWrite);
    // Write to current file position: start of file
    // first write setting file version
    fWrite.WriteInt16L(KTouchUiSettingsVersion);
    // then start writing settings
    fWrite.WriteInt16L(iShowtoolbar);
    fWrite.WriteInt16L(iAllowMouseGrab);
    fWrite.WriteInt16L(iSingleTap);
    fWrite.WriteInt16L(iDoubleTap);
    fWrite.WriteInt16L(iLongTap);
    fWrite.WriteInt16L(iSwipeLeft);
    fWrite.WriteInt16L(iSwipeRight);
    fWrite.WriteInt16L(iSwipeUp);
    fWrite.WriteInt16L(iSwipeDown);
    
    fWrite.WriteInt16L(itbButton1);
    fWrite.WriteInt16L(itbButton2);
    fWrite.WriteInt16L(itbButton3);
    fWrite.WriteInt16L(itbButton4);
    fWrite.WriteInt16L(itbButton5);
    fWrite.WriteInt16L(itbButton6);
    fWrite.WriteInt16L(itbButton7);
    fWrite.WriteInt16L(itbButton8);
    
    fWrite.WriteInt16L(itbButtonCount);
    fWrite.WriteInt16L(itbButtonWidth);
    fWrite.WriteInt16L(itbButtonHeigth);  

    fWrite.WriteInt16L(iButtonUpBackgroundTransparency);
    fWrite.WriteInt16L(iButtonUpTextTransparency);
    fWrite.WriteInt16L(iButtonDownBackgroundTransparency);
    fWrite.WriteInt16L(iButtonDownTextTransparency);
    fWrite.WriteInt16L(iButtonFontSize);

    fWrite.WriteInt16L(iSaveLandscapeMovePoint.iX);        
    fWrite.WriteInt16L(iSaveLandscapeMovePoint.iY);        
    if (iSaveLandscapeIsStartPlaceTop) {
        fWrite.WriteInt16L(1);
    } else {
        fWrite.WriteInt16L(0);
    }
    
    fWrite.WriteInt16L(iSavePortraitMovePoint.iX);
    fWrite.WriteInt16L(iSavePortraitMovePoint.iY);
    if (iSavePortraitIsStartPlaceTop) {
        fWrite.WriteInt16L(1);
    } else {
        fWrite.WriteInt16L(0);
    }
    
    fWrite.CommitL();
    CleanupStack::PopAndDestroy(&fWrite);
}
