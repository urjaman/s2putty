/*    customtoolbar.cpp
 *
 * Putty custom toolbar class for touch UI
 *
 * Copyright 2009 Risto Avila
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <e32cmn.h>
#include <eikenv.h>
#include <eikspane.h>
#include <putty.rsg>
#include <akniconutils.h>
#include <akndef.h>
#include <aknscreenmode.h>
#include <fbs.h>
#include "customtoolbar.h"
#include "terminalview.h"


CCustomToolBar::CCustomToolBar (CTerminalContainer* aParent) : iParent(aParent) {
    // No implementation required
}

CCustomToolBar::~CCustomToolBar ( ) {
    UnloadButtonsL(); //Delete button icons
    iButtonsArray.ResetAndDestroy();
}

void CCustomToolBar::UnloadButtonsL() {


}

CCustomToolBar* CCustomToolBar::NewLC (CTerminalContainer* aParent, TRect aRect, TTouchSettings *aTouchSettings) {
    CCustomToolBar* self = new (ELeave)CCustomToolBar(aParent);
    CleanupStack::PushL (self );
    self->ConstructL (aParent,aRect,aTouchSettings);
    return self;
}

CCustomToolBar* CCustomToolBar::NewL (CTerminalContainer* aParent, TRect aRect, TTouchSettings *aTouchSettings) {
    CCustomToolBar* self=CCustomToolBar::NewLC (aParent,aRect,aTouchSettings);
    CleanupStack::Pop ( ); // self;
    return self;
}

void CCustomToolBar::ConstructL (CTerminalContainer* aParent, TRect /*aRect*/, TTouchSettings *aTouchSettings) {
    iTouchSettings = aTouchSettings;
    
    iDefaultButtonsCount = CountDefaultButtons();
        
    for ( int i = 0 ; i < iDefaultButtonsCount ;i++ ) {      
       TRect buttonInitialSize = TRect(TPoint(0,0),TSize(iTouchSettings->GetTbButtonWidth(), iTouchSettings->GetTbButtonHeigth()));
       CCustomToolbarButton *tmp = CCustomToolbarButton::NewL(buttonInitialSize, KDefaultToolbarButtons[i]);
       iButtonsArray.AppendL(tmp);
    }

    //Defaults
    iSaveLandscapeMovePoint.iX = 0;
    iSaveLandscapeMovePoint.iY = 0;
    iSaveLandscapeIsStartPlaceTop = ETrue;
    
    iSavePortraitMovePoint.iX = 0;
    iSavePortraitMovePoint.iY = 0;
    iSavePortraitIsStartPlaceTop = ETrue;
    
    //upper values gets overwriten from settings file if those exists.
    LoadButtonsL(); //Create button lists
    
    iLocked = ETrue; // locks toolbar to it's place
    if (iTouchSettings->GetShowToolbar() == 1) {
        iHidden = EFalse; // Toolbar is shown on startup
    } else {
        iHidden = ETrue; // Toolbar is hidden on startup
    }

    iTouchFeedBack = MTouchFeedback::Instance();
    iTouchFeedBack->SetFeedbackEnabledForThisApp(ETrue);
    iToolbarOrientation = ELandscape;
    SetContainerWindowL(static_cast<CCoeControl&>(*aParent));
}

void CCustomToolBar::Draw( const TRect& /*aRect*/ ) const {

    if (iHidden) {
      return; // Lets exit if toolbar is hidden
    }
        
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();
 
    // Background of the toolbar -----------------------------------
    TRect toolbarRect = TRect(TPoint(0,0),Size());

    // All buttons---------------------------------------------------
    TRect buttonRect = TRect(TPoint(0,0),TSize(iToolbarItemWidth,iToolbarItemHeight));
    

    for ( int i = 0; i < iToolbarItemCount; i++) {
        
        CCustomToolbarButton *tmp = GetButton(i);
        if ( tmp->GetButtonDown() ) {
            gc.BitBltMasked(tmp->GetRect().iTl, tmp->GetButtonDownBitmap(), buttonRect, tmp->GetButtonDownBitmapMask(), EFalse);
        } else {
            gc.BitBltMasked(tmp->GetRect().iTl, tmp->GetButtonUpBitmap(), buttonRect, tmp->GetButtonUpBitmapMask(), EFalse);
        }
      
    }
    
}

void CCustomToolBar::SizeChanged() {
    // Calculate new item positions
    TRect item = Rect();
        
    // When terminal is in portrait mode
   
   if ( GetOrientation() == EPortrait ) {
        if ( iToolbarOrientation == EPortrait ) {       
            // Toolbar is in portrait mode
           if ( TestForSecondRow( ETestOnly ) ) {
               ToolbarPortrait2rows();
           } else {               
               ToolbarPortrait1rows();
           }
           
        } else {
           //Toolbar is in landscape mode
           if ( TestForSecondRow( ETestOnly ) ) {
               ToolbarLandscape2rows();
           } else {
               ToolbarLandscape1rows(); 
           }
        }
    } else {
        // When terminal is in landscape mode
        if ( iToolbarOrientation == EPortrait ) {
            // Toolbar is in portrait mode
            if ( TestForSecondRow( ETestOnly ) ) {
                ToolbarPortrait2rows();
            } else {
                ToolbarPortrait1rows();
            }
            
        } else {
            // Toolbar is in landscape mode
            if ( TestForSecondRow( ETestOnly ) ) {
                ToolbarLandscape2rows();
            } else {
                ToolbarLandscape1rows();
            }
        }
    }
}

void CCustomToolBar::Move(const TPoint& aPoint) {
    // User is moving/dragging this toolbar
    TPoint offset;
    offset.iX =  aPoint.iX - iMovePoint.iX;
    offset.iY = aPoint.iY - iMovePoint.iY;
    
    TRect rect = Rect();
    rect.Move(offset);
    
    if (rect.iTl.iX >= KToolbarGap && rect.iTl.iY >= KToolbarGap && (rect.iBr.iY + KToolbarGap ) <= iParent->Size().iHeight && (rect.iBr.iX + KToolbarGap) <= iParent->Size().iWidth) {
        // Move
        SetRect(rect);
    } else {
        if (rect.iTl.iY < KToolbarGap) {
            SetTop();
            /*
            if ( GetOrientation() == EPortrait ) {
                iSavePortraitIsStartPlaceTop = ETrue;    
            } else {
                iSaveLandscapeIsStartPlaceTop = ETrue;
            }*/
        } else if (rect.iTl.iX < KToolbarGap) {
            SetLeft();
            /*
            if ( GetOrientation() == EPortrait ) {
                iSavePortraitIsStartPlaceTop = EFalse;    
            } else {
                iSaveLandscapeIsStartPlaceTop = EFalse;
            }*/
        }
    }
   
}

TBool CCustomToolBar::IsDragDone(const TPointerEvent& aPointerEvent) {
    // We accept dragging when user has done at least KDraggingAccepted pixel drag
    TBool ret = EFalse;
    
    TInt x = Abs(aPointerEvent.iPosition.iX - iDragStartPoint.iX);
    TInt y = Abs(aPointerEvent.iPosition.iY - iDragStartPoint.iY);

    if (x > KDraggingAccepted || y > KDraggingAccepted) {
        ret  = ETrue;
    }
    
    return ret;
}

void CCustomToolBar::HandlePointerEventL(const TPointerEvent& aPointerEvent) {

    if (iHidden) {
       return; // Lets exit if hidden
    }
        
    if (aPointerEvent.iType == TPointerEvent::EButton1Down) {
        iDrag = EFalse;
        iMovePoint = aPointerEvent.iPosition;
        iDragStartPoint = aPointerEvent.iPosition;
        
        //Draw button down if not selectable tool. Do we need this after code clean up?
        for ( int i = 0; i < iToolbarItemCount; i++) {
            if ( GetButton(i)->GetRect().Contains(aPointerEvent.iPosition) ) {
                if ( GetButton(i)->GetButtonSelectable() ) {
                    iTool = GetButtonData(i).iAction;
                    GetButton(i)->SetButtonDown(ETrue);
                    iLastButtonDown = GetButton(i)->GetRect();
                }
                iTouchFeedBack->InstantFeedback(ETouchFeedbackBasic); // touch feedback to all buttons down
            }
        }
        DrawDeferred();
    } else if (aPointerEvent.iType == TPointerEvent::EDrag) {
        if (!iLocked) {
            // Do we accept dragging?
            if (!iDrag && IsDragDone(aPointerEvent)) {
                iDrag = ETrue;
                
                if ( iTool == EPuttyToolbarSelect ) {
                    for (int i = 0 ; i < iToolbarItemCount ; i++) {
                        if ( GetButtonData(i).iAction == EPuttyToolbarSelect) {
                            GetButton(i)->SetButtonDown(EFalse);
                            break;
                        }
                    }
                    iParent->HandleCustomToolbar(EPuttyToolbarSelect); //inform parent that we are moving the toolbar and canceling select
                }
                iTool = EPuttyToolbarNoTool;
                DrawDeferred();
            }

            // Dragging accepted in IsDragDone()
            if (iDrag) {
                TRect acceptedArea = Rect();
                // Grow a little area of where dragging is accepted
                acceptedArea.Grow(KDraggingAcceptGrow,KDraggingAcceptGrow);
                if (acceptedArea.Contains(aPointerEvent.iPosition)) {
                    Move(aPointerEvent.iPosition);
                    iMovePoint = aPointerEvent.iPosition;
                }
            }
        } else {
            //Drag is detected even after 1 pixel move so lets see if the user stays inside the button field
            
            if (!iDrag && IsDragDone(aPointerEvent) && !iLastButtonDown.Contains(aPointerEvent.iPosition) ) {
                if (iTool != EPuttyToolbarSelect) {    
                    iTool = EPuttyToolbarNoTool;
                    for (int i = 0 ; i < iToolbarItemCount; i++) {
                        GetButton(i)->SetButtonDown(EFalse); // put all buttons up
                    }
                }
            }
        }
    } else if (aPointerEvent.iType == TPointerEvent::EButton1Up) {
        if (!iDrag) {
            // Handle tool selection
            for (TInt i = 0; i < iToolbarItemCount; i++) {
                if ( GetButton(i)->GetRect().Contains(aPointerEvent.iPosition) ) {
                    HandleButtonSelection(i);
                    break;
                }
            }
        }
        iDrag = EFalse;
    }
}

void CCustomToolBar::DeActiveTool(TPuttyToolbarCommands aTool) {
    if ( aTool == EPuttyToolbarNoTool ) {
        for ( int i = 0 ; i < iToolbarItemCount; i++) {
            if ( GetButtonData(i).iAction == iTool && GetButtonData(i).iAction != EPuttyToolbarLock ) {
                GetButton(i)->SetButtonDown(EFalse);
            }
        }
        iTool = aTool;
        return;
    } else {
        for ( int i = 0; i < iToolbarItemCount; i++) {
            if ( GetButtonData(i).iAction == aTool && GetButtonData(i).iAction != EPuttyToolbarLock ) {
                GetButton(i)->SetButtonDown(EFalse);
                if ( iTool == aTool ) {
                    iTool = EPuttyToolbarNoTool;
                }
                break;
            }

        }
    
    }
    //iTool = aTool;
}

void CCustomToolBar::ActivateTool(TPuttyToolbarCommands aTool) {
     for ( int i = 0; i < iToolbarItemCount; i++) {
            if ( GetButtonData(i).iAction == aTool && GetButtonData(i).iAction != EPuttyToolbarLock ) {
                GetButton(i)->SetButtonDown(ETrue);
                iTool = aTool;
                break;
            }
        }    
}

//Handles real button press
void CCustomToolBar::HandleButtonSelection(TInt aButtonPos) {
    
    switch (GetButtonData(aButtonPos).iAction) {
        case EPuttyToolbarTab:
            iParent->HandleCustomToolbar(EPuttyToolbarTab);
            iTool = EPuttyToolbarNoTool;
            GetButton(aButtonPos)->SetButtonDown(EFalse);
            break;
        case EPuttyToolbarAltP:
            iTool = EPuttyToolbarAltP;
            GetButton(aButtonPos)->SetButtonDown(ETrue);
            iParent->HandleCustomToolbar(EPuttyToolbarAltP);
            break;
        case EPuttyToolbarCtrlP:
            iTool = EPuttyToolbarCtrlP;
            GetButton(aButtonPos)->SetButtonDown(ETrue);
            iParent->HandleCustomToolbar(EPuttyToolbarCtrlP);
            break;
        case EPuttyToolbarLock:
            if (iLocked) {
              SetLock(EFalse);
            } else {
              SetLock(ETrue);
            }
            break;
        case EPuttyToolbarSelect:
            iTool = EPuttyToolbarSelect;
            GetButton(aButtonPos)->SetButtonDown(ETrue);
            iParent->HandleCustomToolbar(EPuttyToolbarSelect);
            break;
        default:
        /*
        case EPuttyToolbarPipe:
        case EPuttyToolbarArrowUp:
        case EPuttyToolbarArrowDown:
        case EPuttyToolbarArrowLeft:
        case EPuttyToolbarArrowRight:
        case EPuttyToolbarEsc:
        case EPuttyToolbarPageUp:
        case EPuttyToolbarPageDown:
        case EPuttyToolbarHome:
        case EPuttyToolbarEnd:
        case EPuttyToolbarDelete:
        case EPuttyToolbarInsert:
        case EPuttyToolbarEnter:
        */
            iParent->HandleCustomToolbar(GetButtonData(aButtonPos).iAction);
            iTool = EPuttyToolbarNoTool;
            GetButton(aButtonPos)->SetButtonDown(EFalse);
            break;
    }
}

void CCustomToolBar::SetLock(TBool aLock) {
    iLocked = aLock; //Set lock
    
    if ( iLocked ) { //toolbar locked overwrite starting point with offset
        TRect tbRect = Rect();
        if (GetOrientation() == ELandscape ) {
            iSaveLandscapeMovePoint = tbRect.iTl; //Save top left position            
            iTouchSettings->SetTbLandscapeMovePoint(iSaveLandscapeMovePoint);
            iTouchSettings->SetTbLandscapeStartPlace(iSaveLandscapeIsStartPlaceTop);
        } else {
            iSavePortraitMovePoint = tbRect.iTl;           
            iTouchSettings->SetTbPortraitMovePoint(iSavePortraitMovePoint);
            iTouchSettings->SetTbPortraitStartPlace(iSavePortraitIsStartPlaceTop);
        }        
    }
        
    //Update button state if visible
    TBool iFound = EFalse;
    TInt i = 0;
    for (  i = 0 ; i < iToolbarItemCount ; i++ ) {
       if ( GetButtonData(i).iAction == EPuttyToolbarLock) {
           iFound = ETrue;
           break;
       }
    }
    if ( iFound ) {      
        if ( iLocked ) {
            //set button up
            GetButton(i)->SetButtonDown(EFalse);
        } else {
            //set button down
            GetButton(i)->SetButtonDown(ETrue);
        }
        DrawDeferred();
    }
}

void CCustomToolBar::LoadButtonsL() {
    UpdateButtonsSizeAndCount(iTouchSettings->GetTbButtonWidth(),iTouchSettings->GetTbButtonHeigth(),iTouchSettings->GetTbButtonCount());
    
    
    for ( TInt i = 0 ; i < iButtonsArray.Count() ; i++ ) {
    
       switch ( iButtonsArray.operator [](i)->GetButtonData().iAction ) {
           case EPuttyToolbarAltP:
           case EPuttyToolbarCtrlP:
           case EPuttyToolbarLock:
           case EPuttyToolbarSelect:
               iButtonsArray.operator [](i)->SetButtonSelectable(EFalse);
               break;
           default:
               iButtonsArray.operator [](i)->SetButtonSelectable(ETrue);
       }
    }    

    SetDefaultButtonsFromSettings(); // Set initial values
}

void CCustomToolBar::SetDefaultButtonsFromSettings() {
    
    iToolbarButtons[0] = iTouchSettings->GetTbButton1();
    iToolbarButtons[1] = iTouchSettings->GetTbButton2();
    iToolbarButtons[2] = iTouchSettings->GetTbButton3();
    iToolbarButtons[3] = iTouchSettings->GetTbButton4();
    iToolbarButtons[4] = iTouchSettings->GetTbButton5();
    iToolbarButtons[5] = iTouchSettings->GetTbButton6();
    iToolbarButtons[6] = iTouchSettings->GetTbButton7();
    iToolbarButtons[7] = iTouchSettings->GetTbButton8();
    
    TInt fontSize = iTouchSettings->GetButtonFontSize();;
    if ( fontSize == 0 ) {
        fontSize = CalculateBestFontSize();
    } 
    
    for (int i = 0 ; i < iToolbarItemCount ; i++) {
        GetButton(i)->GenerateIconUpL(fontSize, iTouchSettings->GetButtonUpBGTransparency(), iTouchSettings->GetButtonUpTextTransparency());
        GetButton(i)->GenerateIconDownL(fontSize, iTouchSettings->GetButtonDownBGTransparency(), iTouchSettings->GetButtonDownTextTransparency());
    }
    
    iSaveLandscapeMovePoint = iTouchSettings->GetTbLandscapeMovePoint();        
    iSaveLandscapeIsStartPlaceTop = iTouchSettings->GetTbLandscapeStartPlace();
    iSavePortraitMovePoint = iTouchSettings->GetTbPortraitMovePoint();
    iSavePortraitIsStartPlaceTop = iTouchSettings->GetTbPortraitStartPlace();    
}

void CCustomToolBar::SwapButtons(TInt aButton, TInt aCommand) {
    TRect tmpItem = GetButton(aButton)->GetRect();
    TInt tmpButton = iToolbarButtons[aButton];
    
    for ( int i = 0 ; i < iToolbarItemCount; i++) {
        if ( GetButtonData(i).iAction == aCommand) {
            //Change button places on screen
            GetButton(aButton)->SetRect(GetButton(i)->GetRect());
            GetButton(i)->SetRect(tmpItem);
            iToolbarButtons[aButton] = iToolbarButtons[i];       
            iToolbarButtons[i] = tmpButton;
            return;
        }
    }
}

void CCustomToolBar::UpdateButtonsFromSettingsL() {
    UpdateButtonsSizeAndCount(iTouchSettings->GetTbButtonWidth(),iTouchSettings->GetTbButtonHeigth(),iTouchSettings->GetTbButtonCount());
    
    if (iToolbarButtons[0] != iTouchSettings->GetTbButton1()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton1()))->SetRect(GetButton(0)->GetRect());
        iToolbarButtons[0] = iTouchSettings->GetTbButton1();
    }
    
    if (iToolbarButtons[1] != iTouchSettings->GetTbButton2()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton2()))->SetRect(GetButton(1)->GetRect());
        iToolbarButtons[1] = iTouchSettings->GetTbButton2();
    }

    if (iToolbarButtons[2] != iTouchSettings->GetTbButton3()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton3()))->SetRect(GetButton(2)->GetRect());
        iToolbarButtons[2] = iTouchSettings->GetTbButton3();
    }
    
    if (iToolbarButtons[3] != iTouchSettings->GetTbButton4()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton4()))->SetRect(GetButton(3)->GetRect());
        iToolbarButtons[3] = iTouchSettings->GetTbButton4();
    }

    if (iToolbarButtons[4] != iTouchSettings->GetTbButton5()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton5()))->SetRect(GetButton(4)->GetRect());
        iToolbarButtons[4] = iTouchSettings->GetTbButton5();
    }
    
    if (iToolbarButtons[5] != iTouchSettings->GetTbButton6()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton6()))->SetRect(GetButton(5)->GetRect());
        iToolbarButtons[5] = iTouchSettings->GetTbButton6();
    }
    
    if (iToolbarButtons[6] != iTouchSettings->GetTbButton7()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton7()))->SetRect(GetButton(6)->GetRect());
        iToolbarButtons[6] = iTouchSettings->GetTbButton7();
    }
    
    if (iToolbarButtons[7] != iTouchSettings->GetTbButton8()) {
        (iButtonsArray.operator [](iTouchSettings->GetTbButton8()))->SetRect(GetButton(7)->GetRect());
        iToolbarButtons[7] = iTouchSettings->GetTbButton8();
    }
    
    
    for ( TInt i = 0 ; i< iToolbarItemCount ; i++ ) {
        TRect tmp = GetButton(i)->GetRect();
        tmp.SetSize(TSize(iTouchSettings->GetTbButtonWidth(),iTouchSettings->GetTbButtonHeigth()));
        GetButton(i)->SetRect(tmp);
    }
    
    TInt fontSize = iTouchSettings->GetButtonFontSize();
    if ( fontSize == 0 ) {
        fontSize = CalculateBestFontSize();
    } 
    
    for (TInt i = 0 ; i < iToolbarItemCount ; i++) {
        GetButton(i)->GenerateIconUpL(fontSize, iTouchSettings->GetButtonUpBGTransparency(), iTouchSettings->GetButtonUpTextTransparency());
        GetButton(i)->GenerateIconDownL(fontSize, iTouchSettings->GetButtonDownBGTransparency(), iTouchSettings->GetButtonDownTextTransparency());
    }
    
    SetLastSavedPosition();
    
    SetLock(iLocked); //Unlock/Lock button might be back in the toolbar so we need to set it into correct state. 
}

void CCustomToolBar::SetLastSavedPosition() {
    //SetTop(); //Set top first and then move to last known place.
    iMovePoint.iX = 0;
    iMovePoint.iY = 0;
    switch ( GetOrientation() ) {
        case ELandscape: {
            if ( iSaveLandscapeIsStartPlaceTop ) {
                SetTop();
            } else {           
                SetLeft(); //if false rearange toolbar to left.
            }
            if ( iSaveLandscapeMovePoint.iX != 0 || iSaveLandscapeMovePoint.iY != 0 ) {
                TRect tbRect(iSaveLandscapeMovePoint,Rect().Size());
                SetRect(tbRect);
            }
            break;
        }
        case EPortrait: {
           if ( iSavePortraitIsStartPlaceTop ) {
               SetTop();
           } else {
               SetLeft(); //if false rearange toolbar to left.
           }
           if ( iSavePortraitMovePoint.iY != 0 || iSavePortraitMovePoint.iX != 0 ) {
               TRect tbRect(iSavePortraitMovePoint,Rect().Size());
               SetRect(tbRect);
           }
           break;
        }
    }    
}

void CCustomToolBar::SetTop() {
    // Set toolbar location in screen
    TRect rect;
    iToolbarOrientation = ELandscape;
    if ( GetOrientation() == ELandscape )
        {
            // Set toolbar to TOP in terminal landscape mode
            iSaveLandscapeIsStartPlaceTop = ETrue;
            if ( TestForSecondRow( ELandscape ) ) {
                TInt testWidth = (iParent->Size().iWidth - iLandscapeToolbarWidth2Rows)/2;
                if ( testWidth < 0 ) testWidth = KToolbarGap;
                rect = TRect(TPoint(testWidth,KToolbarGap), TSize(iLandscapeToolbarWidth2Rows,iLandscapeToolbarHeight2Rows));                
            } else {
                TInt testWidth = (iParent->Size().iWidth - iLandscapeToolbarWidth)/2;
                if ( testWidth < 0 ) testWidth = KToolbarGap;
                rect = TRect(TPoint(testWidth,KToolbarGap), TSize(iLandscapeToolbarWidth,iLandscapeToolbarHeight));
            }        
        }
    else
        {
            // Set toolbar to TOP in terminal portrait mode
            iSavePortraitIsStartPlaceTop = ETrue;
            if ( TestForSecondRow( ELandscape ) ) {
                TInt testWidth = (iParent->Size().iWidth - iLandscapeToolbarWidth2Rows)/2;
                if ( testWidth < 0 ) testWidth = KToolbarGap;
                rect = TRect(TPoint(testWidth,KToolbarGap), TSize(iLandscapeToolbarWidth2Rows,iLandscapeToolbarHeight2Rows));           
            } else {
                TInt testWidth = (iParent->Size().iWidth - iLandscapeToolbarWidth)/2;
                if ( testWidth < 0 ) testWidth = KToolbarGap;
                rect = TRect(TPoint(testWidth,KToolbarGap), TSize(iLandscapeToolbarWidth,iLandscapeToolbarHeight));
            }        
        }
    iTouchFeedBack->InstantFeedback(ETouchFeedbackBasic); // touch feedback
    SetRect(rect);
}

void CCustomToolBar::SetLeft() {
    // Set toolbar location in screen
    TRect rect;
    iToolbarOrientation = EPortrait;
    if ( GetOrientation() == ELandscape ) {
        // Set toolbar to LEFT in terminal landscape mode   
        iSaveLandscapeIsStartPlaceTop = EFalse;
        if ( TestForSecondRow( EPortrait ) ) {
            //TInt count = iToolbarItemCount/2;
            TInt testHeigth = (iParent->Size().iHeight - iPortraitToolbarHeigth2Rows)/2;
            if ( testHeigth < 0 ) testHeigth = KToolbarGap;
            rect = TRect(TPoint(KToolbarGap,testHeigth), TSize(iPortraitToolbarWidth2Rows, iPortraitToolbarHeigth2Rows));
        } else {
            TInt testHeigth = (iParent->Size().iHeight - iPortraitToolbarHeigth)/2;
            if ( testHeigth < 0 ) testHeigth = KToolbarGap;
            rect = TRect(TPoint(KToolbarGap,testHeigth), TSize(iPortraitToolbarWidth,iPortraitToolbarHeigth));        
        }
    } else {
        // Set toolbar to LEFT in terminal portrait mode
        iSavePortraitIsStartPlaceTop = EFalse;
        if ( TestForSecondRow( EPortrait ) ) {
            TInt testHeigth = (iParent->Size().iHeight - iPortraitToolbarHeigth2Rows)/2;
            if ( testHeigth < 0 ) testHeigth = KToolbarGap;
            rect = TRect(TPoint(KToolbarGap,testHeigth), TSize(iPortraitToolbarWidth2Rows,iPortraitToolbarHeigth2Rows));
        
        } else {
            TInt testHeigth = (iParent->Size().iHeight - iPortraitToolbarHeigth)/2;
            if ( testHeigth < 0 ) testHeigth = KToolbarGap;
            rect = TRect(TPoint(KToolbarGap,testHeigth), TSize(iPortraitToolbarWidth,iPortraitToolbarHeigth));
        } 
    }
    iTouchFeedBack->InstantFeedback(ETouchFeedbackBasic); // touch feedback
    SetRect(rect);
}

TOrientation CCustomToolBar::GetOrientation() {
    TRect test = CEikonEnv::Static()->EikAppUi()->ApplicationRect();
    //TInt widht = test.Size().iWidth;
    TInt heigth = test.Size().iHeight;
    if ( heigth == KPortraitHeight ) {
        return EPortrait;
    } else {
        return ELandscape;
    }  
}

TBool CCustomToolBar::TestForSecondRow(TOrientation aToolbarOrientation) {
    TRect item = iParent->Rect();
    
    if ( GetOrientation() == EPortrait ) {
        if ( aToolbarOrientation == ETestOnly ) {
            if ( iToolbarOrientation == EPortrait ) {  
                // Toolbar is in portrait mode
                if ( item.Height() >=  iPortraitToolbarHeigth ) {
                    //Height is enough
                    return EFalse;
                } else {
                    return ETrue;
                }
            } else {
                // Toolbar is in landscape mode
                if ( item.Width() >= iLandscapeToolbarWidth ) {
                    return EFalse;
                } else {
                    return ETrue;
                }
            }
        } else {
            if ( aToolbarOrientation == EPortrait ) {
                //We want to set toolbar to portrait mode so we must test for portrait
                if ( item.Height() >=  iPortraitToolbarHeigth ) {
                    //Height is enough
                    return EFalse;
                } else {
                    return ETrue;
                }
            } else {
                // set toolbar to landscape mode
                if ( item.Width() >= iLandscapeToolbarWidth ) {
                    return EFalse;
                } else {
                    return ETrue;
                }            
            }
        }
    } else {
        if ( aToolbarOrientation == ETestOnly ) {
            if ( iToolbarOrientation == EPortrait ) {  
                // Toolbar is in portrait mode
                if ( item.Height() >=  iPortraitToolbarHeigth ) {
                    return EFalse;
                } else {
                    return ETrue;
                }
            } else {
                // Toolbar is in landscape mode
                if ( item.Width() >= iLandscapeToolbarWidth ) {
                    return EFalse;
                } else {
                    return ETrue;
                }
            }
        } else {
            if ( aToolbarOrientation == EPortrait ) {
                //We want to set toolbar to portrait mode so we must test for portrait
                if ( item.Height() >=  iPortraitToolbarHeigth ) {
                    //heigth is enough
                    return EFalse;
                } else {
                   return ETrue;
                }           
            } else {
                //set toolbar to landscape mode
                if ( item.Width() >= iLandscapeToolbarWidth ) {
                    return EFalse;
                } else {
                    return ETrue;
                }                
            }        
        }
    }
}

void CCustomToolBar::UpdateButtonsSizeAndCount(TInt aWidth, TInt aHeigth, TInt aCount) {
    // Toolbar button width
    iToolbarItemWidth = aWidth;

    // Toolbar button height
    iToolbarItemHeight = aHeigth;

    // Amount of buttons in the toolbar
    iToolbarItemCount = aCount;

    TInt iToolbarHalfCount = iToolbarItemCount/2;
    
    //Toolbar portrait
    //width & heigth when toolbar is in one row
    iPortraitToolbarWidth = iToolbarItemWidth;
    if ( iToolbarItemCount > 0) {
        iPortraitToolbarHeigth = iToolbarItemHeight*iToolbarItemCount + (iToolbarItemCount * KToolbarItemGap);
    } 
    
    //two rows
    if ( (iToolbarItemCount % 2 ) != 0 ) {
        TInt tmpRound = iToolbarItemCount +1;
        TInt tmpRoundHalf = tmpRound/2;
        iPortraitToolbarWidth2Rows = iToolbarItemWidth*2 + KToolbarItemGap;
        if ( tmpRoundHalf > 0) {
            iPortraitToolbarHeigth2Rows = iToolbarItemHeight*tmpRoundHalf + (tmpRoundHalf * KToolbarItemGap);
        }        
    } else {    
        iPortraitToolbarWidth2Rows = iToolbarItemWidth*2 + KToolbarItemGap;
        if ( iToolbarHalfCount > 0) {
            iPortraitToolbarHeigth2Rows = iToolbarItemHeight*iToolbarHalfCount + (iToolbarHalfCount * KToolbarItemGap);
        }
    }
    
    //Toolbar landscape
    //width & heigth when toolbar is in one row
    iLandscapeToolbarHeight = iToolbarItemHeight;
    
    if ( iToolbarItemCount > 0 ) {
        iLandscapeToolbarWidth = iToolbarItemWidth*(iToolbarItemCount) + (iToolbarItemCount * KToolbarItemGap );
    }
    
    //two rows
    if ( (iToolbarItemCount % 2 ) != 0 ) {
        TInt tmpRound = iToolbarItemCount +1;
        TInt tmpRoundHalf = tmpRound/2;
        iLandscapeToolbarHeight2Rows = iToolbarItemHeight*2 + KToolbarItemGap;    
        if ( tmpRoundHalf > 0 ) {
            iLandscapeToolbarWidth2Rows = iToolbarItemWidth*tmpRoundHalf + (tmpRoundHalf * KToolbarItemGap);
        }    
    } else {
        iLandscapeToolbarHeight2Rows = iToolbarItemHeight*2 + KToolbarItemGap;    
        if ( iToolbarHalfCount > 0 ) {
            iLandscapeToolbarWidth2Rows = iToolbarItemWidth*iToolbarHalfCount + (iToolbarHalfCount * KToolbarItemGap);
        }
    }
}

void CCustomToolBar::ToolbarPortrait1rows() {
    TRect item = Rect();
    // Toolbar is in portrait mode
    item.SetWidth(iToolbarItemWidth);
    item.SetHeight(iToolbarItemHeight);

    // Move to toolbar center
    item.Move((iPortraitToolbarWidth-iToolbarItemWidth)/2,0);
            
    item.Move(0,KToolbarItemGap);
 
    for (int i = 0; i < (iToolbarItemCount); i++) {
        GetButton(i)->SetRect(item);
        item.Move(0,iToolbarItemHeight+KToolbarItemGap);
    }
}


void CCustomToolBar::ToolbarPortrait2rows() {
    TRect item = Rect();
    // Toolbar is in portrait mode
    item.SetWidth(iToolbarItemWidth);
    item.SetHeight(iToolbarItemHeight);

    // Move to toolbar center
    item.Move((iPortraitToolbarWidth2Rows-iToolbarItemWidth-iToolbarItemWidth)/2,0);
            
    item.Move(0,KToolbarItemGap);
    
    TInt halfCount = (iToolbarItemCount/2);    
    if ( ( iToolbarItemCount % 2 ) != 0 ) {
        halfCount = halfCount+1;
    }
    
    for (int i = 0; i < halfCount; i++) {
        GetButton(i)->SetRect(item);
        item.Move(0,iToolbarItemWidth+KToolbarItemGap);
    }
                        
    // New item line ------
    item = Rect();
    item.SetWidth(iToolbarItemWidth);
    item.SetHeight(iToolbarItemHeight);
    item.Move(iToolbarItemWidth+KToolbarItemGap +((iPortraitToolbarWidth2Rows-iToolbarItemWidth-iToolbarItemWidth)/2),0);

    item.Move(0,KToolbarItemGap);
            
    for (int i = halfCount; i < (iToolbarItemCount); i++) {
        GetButton(i)->SetRect(item);
        item.Move(0,iToolbarItemWidth+KToolbarItemGap);
    }
}


void CCustomToolBar::ToolbarLandscape2rows() {
    TRect item = Rect();
    item.SetWidth(iToolbarItemWidth);
    item.SetHeight(iToolbarItemHeight);

    // Move to toolbar center
    item.Move(0,(iLandscapeToolbarHeight2Rows-iToolbarItemHeight-iToolbarItemHeight)/2);
            
    item.Move(KToolbarItemGap,0);
    
    TInt halfCount = (iToolbarItemCount/2);    
    if ( ( iToolbarItemCount % 2 ) != 0 ) {
        halfCount = halfCount+1;
    }
    
    for (int i = 0; i < halfCount; i++) {
        GetButton(i)->SetRect(item);
        item.Move(iToolbarItemWidth+KToolbarItemGap,0);
    }
    
    // New item line ------
    item = Rect();
    item.SetWidth(iToolbarItemWidth);
    item.SetHeight(iToolbarItemHeight);
    item.Move(0,iToolbarItemHeight+KToolbarItemGap +((iPortraitToolbarWidth - iToolbarItemWidth)/2));
                        
    item.Move(KToolbarItemGap,0);

    for (int i = halfCount; i < (iToolbarItemCount); i++) {
        GetButton(i)->SetRect(item);
        item.Move(iToolbarItemWidth+KToolbarItemGap,0);
    }
    
}

void CCustomToolBar::ToolbarLandscape1rows() {
    TRect item = Rect();
    // Toolbar is in landscape mode
    item.SetWidth(iToolbarItemWidth);
    item.SetHeight(iToolbarItemHeight);

    // Move to toolbar center
    item.Move(0,(iLandscapeToolbarHeight-iToolbarItemHeight)/2);
            
    item.Move(KToolbarItemGap,0);

    for (int i = 0; i < (iToolbarItemCount); i++) {
        GetButton(i)->SetRect(item);
        item.Move(iToolbarItemWidth+KToolbarItemGap,0);
    }
}

TInt CCustomToolBar::CalculateBestFontSize() {
    TInt iFontSize = GetButton(0)->GetOptimalFontSize();
    
    //Find longest text
    for (int i = 0 ; i < iToolbarItemCount; i++) {
        CCustomToolbarButton *tmp = GetButton(i);
        if ( tmp->GetOptimalFontSize() < iFontSize ) { 
            iFontSize = tmp->GetOptimalFontSize();
        }
    }
    
    return iFontSize;
}

TInt CCustomToolBar::CountDefaultButtons() {
    TInt count = 0;
    
    while (KDefaultToolbarButtons[count].iAction != EPuttyToolbarNoTool ) {
        count++;
    }
    return count;
}
