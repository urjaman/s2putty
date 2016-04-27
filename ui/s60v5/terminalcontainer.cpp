/*    terminalcontainer.cpp
 *
 * Putty UI container class for the terminal view
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2004,2007-2011 Petteri Kangaslampi
 * Copyright 2009 Risto Avila
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <gdi.h>
#include <eikenv.h>
#include <apgtask.h>
#include <eikspane.h>
#include <eikbtgpc.h>
#include <eikmenub.h>

#include <coemain.h>
#include <badesca.h> 
#include <aknbutton.h> 
#include <barsread.h>
#include <aknsdrawutils.h>
#include <aknsbasicbackgroundcontrolcontext.h>
#include <stringloader.h>

#include "puttyappui.h"
#include "terminalview.h"
#include "terminalcontainer.h"
#include "puttyui.hrh"
#include "puttyengine.h"
#include "s2font.h"
#include "sendgrid.h"

#include <putty.rsg>
#include <aknnotewrappers.h> 
#include "customtoolbar.h"

#include <e32debug.h> //this is for rdebug

_LIT(KAssertPanic, "terminalcontainer.cpp");
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KAssertPanic, __LINE__))

const TInt KDoubleTapMin = 50000; //50ms minimum
const TInt KDoubleTapMax = 200000; //200ms maximum

// Factory method
CTerminalContainer *CTerminalContainer::NewL(const TRect &aRect,
                                             MTerminalObserver *aTerminalObserver,
                                             CTerminalView *aView,
                                             const TDesC &aFontFile,
                                             const TDesC &aSettingsFile) {
    
    CTerminalContainer *self = new (ELeave) CTerminalContainer(aView);
    CleanupStack::PushL(self);
    self->ConstructL(aRect, aTerminalObserver, aFontFile, aSettingsFile);
    CleanupStack::Pop(self);
    return self;
}


// Constructor
CTerminalContainer::CTerminalContainer(CTerminalView *aView) {
    iView = aView;
    iDefaultFg = KRgbBlack;
    iDefaultBg = KRgbWhite;
}


// Desctructor
CTerminalContainer::~CTerminalContainer() {
    iDragPoints.Close();
    delete iTerminal;
    delete iFont;
    delete iTimer;
    delete iLongTapDetector;
    delete iPopup;
    delete iCustomToolbarControl;
}


// Second-phase constructor
void CTerminalContainer::ConstructL(const TRect &aRect,
                                    MTerminalObserver *aTerminalObserver,
                                    const TDesC &aFontFile,
                                    const TDesC &aSettingsFile) {

    iSettingsFile = aSettingsFile;
    iTouchSettings.ReadSettingFileL(iSettingsFile);
#ifdef PUTTY_SYM3 //partial screen vkb    
    iSplitView = EFalse; // vkb is not open in split view
#endif
    CreateWindowL();
    iFont = CS2Font::NewL(aFontFile);
    SetRect(aRect);
    DrawableWindow()->SetPointerGrab( ETrue );
    EnableDragEvents();

    iTouchFeedBack = MTouchFeedback::Instance();
    iTouchFeedBack->SetFeedbackEnabledForThisApp(ETrue);
    
    GetTerminalRect(iTermRect);
    iTerminal = CTerminalControlS2Font::NewL(*aTerminalObserver, iTermRect,
                                             Window(), *iFont);
    iTerminal->SetMopParent(this);
    iTerminal->SetFocus(ETrue);
#ifdef PUTTY_SYM3 //partial screen vkb    
    iTerminal->UpdateFlags(); // enable partial screen flag.
#endif
    
    iTimer = COneShotTimer::NewL(TCallBack(DoubleTapCallBack, this));
    iLastTapStart = TTime(0);
    iSingleTapTimerExpired = EFalse;
    iSelectionStart = EFalse;
    iSelectEnabled = EFalse;
    iLongTapDetector = CAknLongTapDetector::NewL( this );
    
    //toolbar
    iCustomToolbarControl = CCustomToolBar::NewL(this,aRect, &iTouchSettings);
    iCustomToolbarControl->SetLastSavedPosition();
    iCustomToolbarControl->SetMopParent(this);
    iCustomToolbarControl->SetFocus(ETrue);
    
    iAltModifier = EFalse;
    iCtrlModifier = EFalse;
}


// Set the font to use
void CTerminalContainer::SetFontL(const TDesC &aFontFile) {

    CS2Font *newFont = CS2Font::NewL(aFontFile);
    CleanupStack::PushL(newFont);    
    iTerminal->SetFontL(*newFont);
    delete iFont;
    iFont = newFont;
    CleanupStack::Pop();
    SizeChanged(); // recalculate terminal rect
}


// Set default colors
void CTerminalContainer::SetDefaultColors(TRgb aForeground, TRgb aBackground) {
    iDefaultFg = aForeground;
    iDefaultBg = aBackground;
    if ( iTerminal ) {
        iTerminal->SetDefaultColors(aForeground, aBackground);
    }
}


// Save UI settings
void CTerminalContainer::SaveSettingsL() {
    iTouchSettings.WriteSettingFileL(iSettingsFile);
}


// Set send grid
void CTerminalContainer::SetSendGrid(CSendGrid *aSendGrid) {
    iSendGrid = aSendGrid;
    iGridTap = EFalse;
}


void CTerminalContainer::Draw(const TRect & /*aRect*/) const {

    CWindowGc &gc = SystemGc();
    gc.Reset();
    gc.SetClippingRect(Rect());

    // Determine terminal window borders
    TRect borderRect = iTermRect;

    // Clear everything outside the terminal
    TRegionFix<5> clearReg(Rect());
    clearReg.SubRect(borderRect);
    assert(!clearReg.CheckError());
    const TRect *rects = clearReg.RectangleList();
    TInt numRects = clearReg.Count();
    
    gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    gc.SetBrushColor(iDefaultBg);
    gc.SetPenStyle(CGraphicsContext::ENullPen);
    while ( numRects-- ) {
        gc.DrawRect(*(rects++));
    }
}


TInt CTerminalContainer::CountComponentControls() const {
    return 2;
}


CCoeControl *CTerminalContainer::ComponentControl(TInt aIndex) const {
    
    switch ( aIndex ) {
        case 0:
            return iTerminal;
        case 1:
            return iCustomToolbarControl;
        default:
            assert(EFalse);
    }

    return NULL;
}


TKeyResponse CTerminalContainer::OfferKeyEventL(const TKeyEvent &aKeyEvent,
                                           TEventCode aType) {

    /*
    // debug help, shows what key was pressed
    CEikonEnv *eenv = CEikonEnv::Static();    
    HBufC *msg = HBufC::NewLC(512);
    TPtr msgp = msg->Des();
    msgp.Format(*eenv->AllocReadResourceLC(R_KEY_EVENT), aKeyEvent.iScanCode, aType);
    CAknInformationNote* dlg = new( ELeave ) CAknInformationNote();
    //dlg->SetTextL( msgp );
    dlg->ExecuteLD(msgp);
    CleanupStack::PopAndDestroy(2); // msg, fmt string
    */

    // Handle a couple of special keys
    if ( aType == EEventKey ) {
        switch ( aKeyEvent.iScanCode ) {
            case EStdKeyDevice3: { // Center of joystick
                // Check if the view wishes to handle this (for copy/paste)
                if ( iView->HandleEnterL() ) {
                    return EKeyWasConsumed;
                } else {                
                    // Send an Enter event to the terminal
                    TKeyEvent event;
                    event.iCode = EKeyEnter;
                    event.iModifiers = 0;
                    event.iRepeats = 0;
                    event.iScanCode = EStdKeyEnter;
                    iTerminal->OfferKeyEventL(event, EEventKey);
                    return EKeyWasConsumed;
                }
            }
                
            case EStdKeyDial:
            case EStdKeyYes: // Green dial button 
                iView->HandleCommandL( EPuttyCmdRepeatLast ); // repeat last command
                return EKeyWasConsumed;
        }
    }

    // Let the terminal handle other keys
    return iTerminal->OfferKeyEventL(aKeyEvent, aType);
}


CTerminalControl &CTerminalContainer::Terminal() {
    return *iTerminal;
}


void CTerminalContainer::GetTerminalRect(TRect &aRect) {
    
    // Get font dimensions
    TInt fontHeight = iFont->FontSize().iHeight;
    TInt fontWidth = iFont->FontSize().iWidth;

    // Terminal maximum size
    TInt termWidth = Rect().Width();
    TInt termHeight = Rect().Height();

    // Round size down to the largest possible terminal that contains whole
    // characters
    termWidth = fontWidth * (termWidth / fontWidth);
    termHeight = fontHeight * (termHeight / fontHeight);
    // The following condition should only be met in the very specific case
    // where an e90 user with 800x352 screen, uses the 14point font designed
    // explicitly for a 80x24 screen size.
    if (fontHeight == 14 && fontWidth == 10 && termWidth == 800) {
        termHeight = 24 * fontHeight;
    }
    assert((termWidth > 0) && (termHeight > 0));

    // Set terminal size and position
    TInt termX = Rect().iTl.iX + (Rect().Width() - termWidth) / 2;
    TInt termY = Rect().iTl.iY + (Rect().Height() - termHeight) / 2;
    aRect.SetRect(TPoint(termX, termY), TSize(termWidth, termHeight));
}

#ifdef PUTTY_SYM3 //partial screen vkb
void CTerminalContainer::SizeChanged() {    
    if ( iSplitView ) {     
      TPoint cur = iTerminal->GetCursor(); //Get cursor position in chars
      TPoint moveTo;
      iMovePoint.iX = 0;
      iMovePoint.iY = (cur.iY * iFont->FontSize().iHeight) + iFont->FontSize().iHeight + iTerminal->Rect().iTl.iY; //set first point to cursor position
      moveTo.iX = 0;
      moveTo.iY = Rect().Height(); //Set next point to actual height
      
      MoveTerminal(moveTo); // Move terminal up as much is needed      
    } else {
        GetTerminalRect(iTermRect);
        if ( iTerminal ) {
            iTerminal->SetRect(iTermRect);
        }
    
        if ( iCustomToolbarControl ) {
            //iCustomToolbarControl->SetLock(ETrue); //Lock toolbar
            iCustomToolbarControl->SetLastSavedPosition();
        }
    }   
    DrawNow(); // Draw it!!
}
#else
void CTerminalContainer::SizeChanged() {    
    GetTerminalRect(iTermRect);
    if ( iTerminal ) {
        iTerminal->SetRect(iTermRect);
    }
    
    if ( iCustomToolbarControl ) {
        iCustomToolbarControl->SetLastSavedPosition();
    }
}

#endif

TCoeInputCapabilities CTerminalContainer::InputCapabilities() const {
    return TCoeInputCapabilities(TCoeInputCapabilities::EAllText |
                                 TCoeInputCapabilities::ENavigation);
}

void CTerminalContainer::SwapButtons(TInt aButton, TInt aCmd) {
    iCustomToolbarControl->SwapButtons(aButton, aCmd);
    iCustomToolbarControl->DrawDeferred();
}

void CTerminalContainer::HandlePointerEventL( const TPointerEvent& aEvent ) {
#ifdef PUTTY_SYM3 //partial screen vkb
    if ( iSplitView ) {
        HandlePointerEventSplitViewL(aEvent);
        return;
    }
#endif
    if ( AknLayoutUtils::PenEnabled() ) {
        iTimer->CancelTimer();

        if ( iSendGrid ) {
            // When send grid is active we'll treat taps outside of the grid
            // as back/cancel and ignore other events
            // (Note that we don't see events inside the grid since it's in
            // front.)
            if ( aEvent.iType == TPointerEvent::EButton1Down ) {
                iGridTap = ETrue;
                iGridTapPoint = aEvent.iPosition;
                return;
            } else if ( (aEvent.iType == TPointerEvent::EButton1Up) &&
                        (iGridTap) ) {
                if ( (Abs(aEvent.iPosition.iX - iGridTapPoint.iX) < 10) &&
                     (Abs(aEvent.iPosition.iY - iGridTapPoint.iY) < 10) ) {
                    iSendGrid->ProcessCommandL(EAknSoftkeyCancel);
                }
                iGridTap = EFalse;
                return;
            } else if ( (aEvent.iType == TPointerEvent::EDrag) && iGridTap ) {
                if ( (Abs(aEvent.iPosition.iX - iGridTapPoint.iX) > 10) ||
                     (Abs(aEvent.iPosition.iY - iGridTapPoint.iY) > 10) ) {
                    // Ignore the tap if it turns into a real drag
                    iGridTap = EFalse;
                }
            }
            return;
        }
        
        //We get button down lets reset callback
        if ( aEvent.iType == TPointerEvent::EButton1Down) {
            iDragPoints.Reset();
            if ( !iCustomToolbarControl->GetHidden() && 
		        iCustomToolbarControl->Rect().Contains(aEvent.iPosition) ) {
                    iIgnoreTbClick = ETrue;
            }
            iLongTapCallbackReceived = EFalse;
            iLastEventDrag = EFalse;
            iDragPoints.Append(aEvent.iPosition);
            DrawDeferred();
        }

        CCoeControl::HandlePointerEventL( aEvent );
        DrawNow();
        
        if ( aEvent.iType == TPointerEvent::EButton1Down) {
            if ( iSelectEnabled ) {
                iView->HandleCommandL(EPuttyCmdMark);
            }
        }
        // Ignore up event coming from toolbar move should we ignore drag event also?
        if ( aEvent.iType == TPointerEvent::EButton1Up && iIgnoreTbClick ) {
            iIgnoreTbClick = EFalse;
            return;
        }
        
        if (iCustomToolbarControl)
            {
            iCustomToolbarControl->DrawNow();
            }        
        
        if ( aEvent.iType == TPointerEvent::EDrag ) {
            if ( iSelectEnabled ) {
                if (!iLastEventDrag) {
                    iLastEventDrag = ETrue;
                } else {
                    iSelectionStart = ETrue;
                }
            }
            iDragPoints.Append(aEvent.iPosition);
        }
                
        TTime now;
        now.HomeTime();
        
        //Double tap
        if ( aEvent.iType == TPointerEvent::EButton1Up && !iLongTapCallbackReceived && now.MicroSecondsFrom(iLastTapStart) < KDoubleTapMax && 
                    now.MicroSecondsFrom(iLastTapStart) > KDoubleTapMin && 
	     (iCustomToolbarControl->GetHidden() || !iCustomToolbarControl->Rect().Contains(aEvent.iPosition)) ) {
                    
            iSingleTapTimerExpired = EFalse;
            iLastEventDrag = EFalse;
            if ( iSelectEnabled ) {
                if (iSelectionStart) {
                    iSelectionStart = EFalse;
                }
            }
            DrawDeferred();
            iLongTapDetector->PointerEventL( aEvent );
            
            HandleTouchAction(EPuttyTouchDoubleTap, aEvent.iPosition);
            //iView->HandleCommandL( EPuttyCmdSend );
            return;
        }
        
        //Test gesture if not in select mode and not in toolbar area
        if ( aEvent.iType == TPointerEvent::EButton1Up && !iSelectEnabled && !iLongTapCallbackReceived) {
            iDragPoints.Append(aEvent.iPosition);
            if (iDragPoints.Count() > 5) {
              TBool isGesture = EFalse;
              switch (IsSwipeGesture()) {
                  case ESwipeLeft:
                      HandleTouchAction(EPuttyTouchSwipeLeft, aEvent.iPosition);
                      isGesture = ETrue;
                      break;
                  case ESwipeRight:
                      HandleTouchAction(EPuttyTouchSwipeRight, aEvent.iPosition);
                      isGesture = ETrue;
                      break;
                  case ESwipeUp:
                      HandleTouchAction(EPuttyTouchSwipeUp, aEvent.iPosition);
                      isGesture = ETrue;
                      break;
                  case ESwipeDown:
                      HandleTouchAction(EPuttyTouchSwipeDown, aEvent.iPosition);
                      isGesture = ETrue;
                      break;
              }
              if (isGesture) {
                  iLongTapDetector->PointerEventL( aEvent );         
                  iCustomToolbarControl->DrawNow(); // redraw toolbar
                  DrawNow();
                  return;
              }
            }
        }

        //Single Tap recieved from timer
        if ( aEvent.iType == TPointerEvent::EButton1Up && !iLongTapCallbackReceived && iSingleTapTimerExpired && 
	     (iCustomToolbarControl->GetHidden() || !iCustomToolbarControl->Rect().Contains(aEvent.iPosition))) {
                    
            iSingleTapTimerExpired = EFalse;
            DrawDeferred();
            iLongTapDetector->PointerEventL( aEvent );
            HandleTouchAction(EPuttyTouchTap, aEvent.iPosition);
            iLastEventDrag = EFalse;
            return;
        }

        //Single Tap timer enable
        if ( aEvent.iType == TPointerEvent::EButton1Up && !iLongTapCallbackReceived &&
	     (iCustomToolbarControl->GetHidden() || !iCustomToolbarControl->Rect().Contains(aEvent.iPosition)) ) {
            iLastPointerEvent = aEvent;
            iSingleTapTimerExpired = EFalse;
            iLastEventDrag = EFalse;
	    if (iView->MouseMode()) {
		    // Do single taps; don't aggregate
		    HandleTouchAction(EPuttyTouchTap, aEvent.iPosition);
	    } else {
		    iTimer->After(KDoubleTapMax); //Start 500ms timer
		    iLastTapStart.HomeTime(); 
	    }
        }
        
        if ( iCustomToolbarControl->GetHidden() || !iCustomToolbarControl->Rect().Contains(aEvent.iPosition) ) {
            iLongTapDetector->PointerEventL( aEvent );   
        }
     }
}

#ifdef PUTTY_SYM3 //partial screen vkb
void CTerminalContainer::HandlePointerEventSplitViewL( const TPointerEvent& aEvent ) {
    //We get down event lets start the move.

    if ( aEvent.iType == TPointerEvent::EButton1Down) {
        iDragPoints.Reset();
        if ( !iCustomToolbarControl->GetHidden() && iCustomToolbarControl->Rect().Contains(aEvent.iPosition) ) {
            iIgnoreTbClick = ETrue;
        }
        iMovePoint = aEvent.iPosition;
        DrawDeferred();
    } 
    
    if ( aEvent.iType != TPointerEvent::EDrag ) {
        CCoeControl::HandlePointerEventL( aEvent ); //Dont send drag events to toolbar!
        DrawNow();
    }

    if ( aEvent.iType == TPointerEvent::EButton1Up && iIgnoreTbClick ) {
        iIgnoreTbClick = EFalse;
        return;
    }        
        
    if ( aEvent.iType == TPointerEvent::EDrag ) {
        MoveTerminal(aEvent.iPosition);
        iMovePoint = aEvent.iPosition;
        DrawDeferred();
    }
}

void CTerminalContainer::MoveTerminal( const TPoint& aPoint ) {
    // User is moving/dragging this toolbar
    TPoint offset;
    //offset.iX =  aPoint.iX - iMovePoint.iX;
    offset.iX = 0;
    offset.iY = aPoint.iY - iMovePoint.iY;
    
    TRect cliRect = Rect();
    TRect rect = iTerminal->Rect();
    rect.Move(offset);
    
    if (rect.iBr.iY >= cliRect.iBr.iY && rect.iTl.iY <= cliRect.iTl.iY  ) {
        // Move
        iTerminal->SetRect(rect);
        //RDebug::Print(_L("Moved terminal")); 
        DrawNow();
    } 
}

#endif

//Returns -1 if no swipe, 1 if swipe right, 0 if swipe left
TSwipeGesture CTerminalContainer::IsSwipeGesture() {
    TSwipeGesture ret;
    
    ret = TestHorizontalSwipe(); //Test left / right
    
    if ( ret == ENoSwipe) {
       ret = TestVerticalSwipe(); //Test up / down
    }
    
    return ret;
}

//Returns -1 if no swipe, 1 if swipe right, 0 if swipe left
TSwipeGesture CTerminalContainer::TestHorizontalSwipe() {
    TPoint firstPoint;
    TPoint lastPoint;

    firstPoint = iDragPoints[0];
    lastPoint = iDragPoints[iDragPoints.Count()-1];

    if ( !iCustomToolbarControl->GetHidden() && iCustomToolbarControl->Rect().Contains(firstPoint) ) {
        return ENoSwipe; //Lets ignore if swipe started from toolbar
    }
    
    // Max difference 33%
    TInt max_y_difference = Abs(firstPoint.iX - lastPoint.iX) / 3;

    // Is line straight?
    // Goes all points throught and finds is some over max_y_difference
    // if found this is not anymore horizontal line
    for (TInt i=1;i<iDragPoints.Count();i++) {
        TPoint point = iDragPoints[i];
        TInt value = Abs(point.iY - firstPoint.iY);
        if (value > max_y_difference) {
            return ENoSwipe;
        }
    }

    // Is direction right or left?
    lastPoint = iDragPoints[iDragPoints.Count()-1];
    if (firstPoint.iX > lastPoint.iX) {
        return ESwipeLeft;
    } else {
        return ESwipeRight;
    }

    //We should not ever get here
    return ENoSwipe;
}

//Returns -1 if no swipe, 2 if swipe up, 3 if swipe down
TSwipeGesture CTerminalContainer::TestVerticalSwipe() {
    TPoint firstPoint;
    TPoint lastPoint;

    firstPoint = iDragPoints[0];
    lastPoint = iDragPoints[iDragPoints.Count()-1];

    if ( !iCustomToolbarControl->GetHidden() && iCustomToolbarControl->Rect().Contains(firstPoint) ) {
        return ENoSwipe; //Lets ignore if swipe started from toolbar
    }
    
    // Max difference 33%
    TInt max_x_difference = Abs(firstPoint.iY - lastPoint.iY) / 3;
    
    // Is line straight?
    // Goes all points throught and finds is some over max_x_difference
    // if found this is not anymore vertical line
    for (TInt i=1;i<iDragPoints.Count();i++) {
        TPoint point = iDragPoints[i];
        TInt value = Abs(point.iX - firstPoint.iX);
        if (value > max_x_difference) {
            return ENoSwipe;
        }
    }

    // Is direction up or down?
    lastPoint = iDragPoints[iDragPoints.Count()-1];
    if (firstPoint.iY > lastPoint.iY) {
        return ESwipeUp;
    } else {
        return ESwipeDown;
    }

    //We should not ever get here
    return ENoSwipe;
}

void CTerminalContainer::HandleLongTapEventL( const TPoint& /* aPenEventLocation */, 
                                      const TPoint& aPenEventScreenLocation ) {
    iLongTapCallbackReceived = ETrue;
    DrawDeferred();
    
    HandleTouchAction(EPuttyTouchLongTap, aPenEventScreenLocation);
}

void CTerminalContainer::UpdateAfterSettingsChangeL() {
    //if buttons are down before editing the buttons set them up before actually updating the buttons
    if ( iAltModifier ) {
        HandleCustomToolbar(EPuttyToolbarAltP);
    }
    if ( iCtrlModifier ) {
        HandleCustomToolbar(EPuttyToolbarCtrlP);    
    }
    if ( iSelectEnabled ) {
        HandleCustomToolbar(EPuttyToolbarSelect);
    }
    
    //it's easier to recreate whole toolbar
    
    iCustomToolbarControl->UpdateButtonsFromSettingsL();
    iCustomToolbarControl->DrawNow();
    
    /*
    iCustomToolbarControl->SetFocus(EFalse);
    delete iCustomToolbarControl;
    iCustomToolbarControl = CCustomToolBar::NewL(this,iTermRect, &iTouchSettings);
    iCustomToolbarControl->SetTop();
    iCustomToolbarControl->SetMopParent(this);
    iCustomToolbarControl->SetFocus(ETrue);
    iCustomToolbarControl->DrawNow();
    */
}

void CTerminalContainer::HandleTouchAction(TInt aCommand, const TPoint& aPenEventScreenLocation) {
    TInt executeCommand = 0;

    if (aCommand == EPuttyTouchTap && iView->MouseMode()) {
	    TInt row = aPenEventScreenLocation.iX / iFont->FontSize().iWidth;
	    TInt col = aPenEventScreenLocation.iY / iFont->FontSize().iHeight;

	    
	    iTouchFeedBack->InstantFeedback(ETouchFeedbackBasic);

	    // We don't get double taps in mouse-mode
	    iView->MouseClick(0x20, row, col);
	    iView->MouseClick(0x23, row, col);
        return;
    }

    switch ( aCommand ) {
        case EPuttyTouchTap:
            executeCommand = iTouchSettings.GetSingleTap();
            break;
        case EPuttyTouchDoubleTap:
            executeCommand = iTouchSettings.GetDoubleTap();
            break;
        case EPuttyTouchLongTap:
            executeCommand = iTouchSettings.GetLongTap();          
            break;
        case EPuttyTouchSwipeLeft:
            executeCommand = iTouchSettings.GetSwipeLeft();
            break;
        case EPuttyTouchSwipeRight:
            executeCommand = iTouchSettings.GetSwipeRight();
            break;           
        case EPuttyTouchSwipeUp:
            executeCommand = iTouchSettings.GetSwipeUp();
            break;
        case EPuttyTouchSwipeDown:
            executeCommand = iTouchSettings.GetSwipeDown();
            break;           
    }
    if (executeCommand != EPuttyCmdNone)
        iTouchFeedBack->InstantFeedback(ETouchFeedbackBasic); // touch feedback
    
    switch ( executeCommand ) {
        case EPuttyCmdStartVKB:
            if ( iSelectEnabled ) {
                if (!iSelectionStart) {
                    iTerminal->StartVKB();
                } else {
                    iSelectionStart = EFalse;
                }
            } else {
                iTerminal->StartVKB();
            }
            break;
        case EPuttyCmdOpenPopUpMenu:
            CreatePopupMenuFromResourceL(aPenEventScreenLocation); 
            break;
        case EPuttyCmdToggleToolbar:
            if ( iCustomToolbarControl->GetHidden() ) {
                iCustomToolbarControl->SetHidden(EFalse);
                iCustomToolbarControl->SetFocus(ETrue);
            } else {
                iCustomToolbarControl->SetHidden(ETrue);
                iCustomToolbarControl->SetFocus(EFalse);
            }
            
            break;
        case EPuttyCmdSendCR:
            iView->HandleCommandL(executeCommand);
            iTerminal->ClearVKBBuffer(); //Clear vkb after enter
            break;
        case EPuttyCmdSendCtrlP:
            iTerminal->SetCtrlModifier(ETrue);
            iView->SetReleaseCtrlAfterKeyPress(ETrue);            
            iTerminal->StartVKB();
            break;
        case EPuttyCmdSendAltP:
            iTerminal->SetAltModifier(ETrue);
            iView->SetReleaseAltAfterKeyPress(ETrue);            
            iTerminal->StartVKB();          
            break;
        default:
            iView->HandleCommandL(executeCommand); // Forward rest of the commands to terminalview
    }
}

void CTerminalContainer::ReleaseAlt() {
        iAltModifier = EFalse;
        iTerminal->SetAltModifier(EFalse);
        iCustomToolbarControl->DeActiveTool(EPuttyToolbarAltP);
        iCustomToolbarControl->DrawDeferred();
}

void CTerminalContainer::ReleaseCtrl() {
        iCtrlModifier = EFalse;
        iTerminal->SetCtrlModifier(EFalse);
        iCustomToolbarControl->DeActiveTool(EPuttyToolbarCtrlP);
        iCustomToolbarControl->DrawDeferred();
}

void CTerminalContainer::Select(TBool aValue) {
    if (!aValue) {
        iTerminal->SetPointerSelect(EFalse);
        iSelectEnabled = EFalse;
        iCustomToolbarControl->DeActiveTool(EPuttyToolbarSelect);
        iCustomToolbarControl->DrawDeferred();
    } else {
        iSelectEnabled = ETrue;
        iTerminal->SetPointerSelect(ETrue);
        iCustomToolbarControl->ActivateTool(EPuttyToolbarSelect);
        iCustomToolbarControl->DrawDeferred();
    }
}

void CTerminalContainer::LockToolbar(TBool aValue) {
    iCustomToolbarControl->SetLock(aValue);
}

void CTerminalContainer::HandleCustomToolbar(TInt aCommand) {
    switch (aCommand) {
        case EPuttyToolbarTab:
            iView->HandleCommandL(EPuttyCmdSendTab);
            break;
        case EPuttyToolbarAltP:
            if ( iAltModifier ) {
                iAltModifier = EFalse;
                iTerminal->SetAltModifier(EFalse);
                iCustomToolbarControl->DeActiveTool(EPuttyToolbarAltP);
                iCustomToolbarControl->DrawDeferred();
            } else {
                iAltModifier = ETrue;
                iTerminal->SetAltModifier(ETrue);
                iTerminal->StartVKB();
            }
            break;
        case EPuttyToolbarCtrlP:
            if ( iCtrlModifier ) {
                iCtrlModifier = EFalse;
                iTerminal->SetCtrlModifier(EFalse);
                iCustomToolbarControl->DeActiveTool(EPuttyToolbarCtrlP);
                iCustomToolbarControl->DrawDeferred();
            } else {
                iCtrlModifier = ETrue;
                iTerminal->SetCtrlModifier(ETrue);
                iTerminal->StartVKB();
            }
            break;
        case EPuttyToolbarSelect:
            iView->HandleCommandL(EPuttyCmdSelect);
            break;
        case EPuttyToolbarCopy:
            iView->HandleCommandL(EPuttyCmdCopy);
            break;
        case EPuttyToolbarPaste:
            iView->HandleCommandL(EPuttyCmdPaste);
            break;
        case EPuttyToolbarPipe:
            iView->HandleCommandL(EPuttyCmdSendPipe);
            break;
        case EPuttyToolbarArrowUp:
            //SendKeypress
            iTerminal->SetLastKeyArrow(); // this is just to keep FEP up to date for console up/down events.
            iView->SendKeypress(EKeyUpArrow,0);
            break;
        case EPuttyToolbarArrowDown:
            iTerminal->SetLastKeyArrow(); // this is just to keep FEP up to date for console up/down events.
            iView->SendKeypress(EKeyDownArrow,0);
            break;
        case EPuttyToolbarArrowLeft:
            iView->SendKeypress(EKeyLeftArrow,0);
            break;
        case EPuttyToolbarArrowRight:
            iView->SendKeypress(EKeyRightArrow,0);
            break;
        case EPuttyToolbarEsc:
            iView->HandleCommandL(EPuttyCmdSendEsc);
            break;
        case EPuttyToolbarPageUp:
            iView->HandleCommandL(EPuttyCmdSendPageUp);
            break;
        case EPuttyToolbarPageDown:
            iView->HandleCommandL(EPuttyCmdSendPageDown);
            break;
        case EPuttyToolbarHome:
            iView->HandleCommandL(EPuttyCmdSendHome);
            break;
        case EPuttyToolbarEnd:
            iView->HandleCommandL(EPuttyCmdSendEnd);
            break;
        case EPuttyToolbarDelete:
            iView->HandleCommandL(EPuttyCmdSendDelete);
            break;
        case EPuttyToolbarInsert:
            iView->HandleCommandL(EPuttyCmdSendInsert);
            break;
        case EPuttyToolbarEnter:
            iTerminal->ClearVKBBuffer(); //Clear vkb after enter            
            iView->HandleCommandL(EPuttyCmdSendCR);         
            break;
        case EPuttyToolbarGrid:
            iTerminal->ClearVKBBuffer(); //Clear vkb if you go to grid
            iTerminal->CloseVKB(); //Close VKB before opening the grid to get correct rect for it.
            iView->HandleCommandL(EPuttyCmdSend);            
            break;
        case EPuttyToolbarListWeb:
            iView->HandleCommandL(EPuttyCmdGrepHttp);
            break;         
    }
}

TInt CTerminalContainer::DoubleTapCallBack(TAny *aPtr) {
    ((CTerminalContainer*)aPtr)->SingleTap();
    return 0;
}

void CTerminalContainer::SingleTap() {
    iSingleTapTimerExpired = ETrue;
    this->HandlePointerEventL(iLastPointerEvent);
}

void CTerminalContainer::CreatePopupMenuFromResourceL( const TPoint &aPosition ) {
    if (iPopup) {
        delete iPopup;
    }
    iSelectionStart = EFalse;
    iLastEventDrag = EFalse;
    iPopup = CAknStylusPopUpMenu::NewL( this, aPosition );
    TResourceReader reader;
    iCoeEnv->CreateResourceReaderLC( reader, R_PUTTY_TERMINAL_POPUP_MENU );
    iPopup->ConstructFromResourceL( reader );
    CleanupStack::PopAndDestroy();
    if ( iCustomToolbarControl->GetLock() ) {
        iPopup->SetItemDimmed(EPuttyCmdLockTb, 1);
    } else {   
        iPopup->SetItemDimmed(EPuttyCmdUnlockTb, 1);
    }
    DrawDeferred();    
    iPopup->ShowMenu();    
}

void CTerminalContainer::ProcessCommandL( TInt aCommandId ) {
    switch( aCommandId ) {
        default:
            iView->HandleCommandL(aCommandId); //Just send all commands to terminalview for now...
    }
    DrawDeferred();
}
