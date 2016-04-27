/*    puttyterminalcontainer.cpp
 *
 * Putty UI container class for the terminal view
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2004 Petteri Kangaslampi
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
#include "puttyappui.h"
#include "puttyterminalview.h"
#include "puttyterminalcontainer.h"
#include "puttyui.hrh"
#include "puttyengine.h"
#include "s2font.h"

#include <putty.rsg>
#include <aknnotewrappers.h> 

_LIT(KAssertPanic, "puttyterminalcontainer.cpp");
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KAssertPanic, __LINE__))


// Factory method
CPuttyTerminalContainer *CPuttyTerminalContainer::NewL(const TRect &aRect,
                                                       MTerminalObserver *aTerminalObserver,
                                                       CPuttyTerminalView *aView,
                                                       const TDesC &aFontFile) {
    
    CPuttyTerminalContainer *self = new (ELeave) CPuttyTerminalContainer(aView);
    CleanupStack::PushL(self);
    self->ConstructL(aRect, aTerminalObserver, aFontFile);
    CleanupStack::Pop(self);
    return self;
}


// Constructor
CPuttyTerminalContainer::CPuttyTerminalContainer(CPuttyTerminalView *aView) {
    iView = aView;
}


// Desctructor
CPuttyTerminalContainer::~CPuttyTerminalContainer() {
    delete iTerminal;
    delete iFont;
}


// Second-phase constructor
void CPuttyTerminalContainer::ConstructL(const TRect &aRect,
                                         MTerminalObserver *aTerminalObserver,
                                         const TDesC &aFontFile) {
    CreateWindowL();
    iFont = CS2Font::NewL(aFontFile);
    SetRect(aRect);

    GetTerminalRect(iTermRect);
    iTerminal = CTerminalControlS2Font::NewL(*aTerminalObserver, iTermRect,
                                             Window(), *iFont);
    iTerminal->SetMopParent(this);
}


// Set the font to use
void CPuttyTerminalContainer::SetFontL(const TDesC &aFontFile) {

    CS2Font *newFont = CS2Font::NewL(aFontFile);
    CleanupStack::PushL(newFont);    
    iTerminal->SetFontL(*newFont);
    delete iFont;
    iFont = newFont;
    CleanupStack::Pop();
    SizeChanged(); // recalculate terminal rect
}


void CPuttyTerminalContainer::Draw(const TRect & /*aRect*/) const {

    CWindowGc &gc = SystemGc();
    gc.Reset();
    gc.SetClippingRect(Rect());

    // Determine terminal window borders and draw a rectangle around it
    TRect borderRect = iTermRect;
    borderRect.Grow(1, 1);
    gc.DrawRect(borderRect);

    // Clear everything outside the terminal
    TRegionFix<5> clearReg(Rect());
    clearReg.SubRect(borderRect);
    assert(!clearReg.CheckError());
    const TRect *rects = clearReg.RectangleList();
    TInt numRects = clearReg.Count();
    
    gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    gc.SetBrushColor(KRgbWhite);
    gc.SetPenStyle(CGraphicsContext::ENullPen);
    while ( numRects-- ) {
        gc.DrawRect(*(rects++));
    }
}


TInt CPuttyTerminalContainer::CountComponentControls() const {
    return 1;
}


CCoeControl *CPuttyTerminalContainer::ComponentControl(TInt aIndex) const {
    
    switch ( aIndex ) {
        case 0:
            return iTerminal;

        default:
            assert(EFalse);
    }

    return NULL;
}


TKeyResponse CPuttyTerminalContainer::OfferKeyEventL(const TKeyEvent &aKeyEvent,
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
                // Send an Enter event to the terminal
                TKeyEvent event;
                event.iCode = EKeyEnter;
                event.iModifiers = 0;
                event.iRepeats = 0;
                event.iScanCode = EStdKeyEnter;
                iTerminal->OfferKeyEventL(event, EEventKey);
                return EKeyWasConsumed;
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


CTerminalControl *CPuttyTerminalContainer::Terminal() {
    return iTerminal;
}


// TDes& CPuttyTerminalContainer::FontName() {
// 	return iFontName;
// }


void CPuttyTerminalContainer::GetTerminalRect(TRect &aRect) {
    
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
    assert((termWidth > 0) && (termHeight > 0));

    // Set terminal size and position
    TInt termX = Rect().iTl.iX + (Rect().Width() - termWidth) / 2;
    TInt termY = Rect().iTl.iY + (Rect().Height() - termHeight) / 2;
    aRect.SetRect(TPoint(termX, termY), TSize(termWidth, termHeight));
}


void CPuttyTerminalContainer::SetFullScreenL(TBool aFullScreen) {

    CEikonEnv *eikonEnv = CEikonEnv::Static();

    iFullScreen = aFullScreen;
    
    if ( iFullScreen ) {
        // Hide indicator bar and CBA and set view to full screen
        eikonEnv->AppUiFactory()->StatusPane()->MakeVisible(EFalse);
        SetRect(iView->ClientRect());        
    } else {
        // Show indicator bar and CBA and reset view
        eikonEnv->AppUiFactory()->StatusPane()->MakeVisible(ETrue);
        SetRect(iView->ClientRect());
    }

    DrawDeferred();
}


void CPuttyTerminalContainer::SetTerminalGrayed(TBool aGrayed) {
    iTerminal->SetGrayed(aGrayed);    
    iTerminal->SetFocus(!aGrayed);
}


void CPuttyTerminalContainer::SizeChanged() {
    GetTerminalRect(iTermRect);
    if ( iTerminal ) {
        iTerminal->SetRect(iTermRect);
    }
    DrawDeferred();
}

TCoeInputCapabilities CPuttyTerminalContainer::InputCapabilities() const {
    return TCoeInputCapabilities(TCoeInputCapabilities::EAllText |
                                 TCoeInputCapabilities::ENavigation);
}
