/*    puttyterminalview.cpp
 *
 * Putty UI View class for the terminal view
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2004 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <aknviewappui.h>
#include <aknconsts.h>
#include <putty.rsg>
#include "puttyterminalview.h"
#include "puttyterminalcontainer.h"
#include "puttyappui.h"


// Factory method
CPuttyTerminalView *CPuttyTerminalView::NewL(MTerminalObserver *aTerminalObserver,
                                             CPuttyAppUi *aAppUi,
                                             const TDesC &aFontFile) {
    
    CPuttyTerminalView *self = new (ELeave) CPuttyTerminalView(aTerminalObserver,
                                                               aAppUi,
                                                               aFontFile);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}


// Constructor
CPuttyTerminalView::CPuttyTerminalView(MTerminalObserver *aTerminalObserver,
                                       CPuttyAppUi *aAppUi,
                                       const TDesC &aFontFile)
    : iAppUi(aAppUi),
      iTerminalObserver(aTerminalObserver) {
    iFontFile = aFontFile;
}


// Destructor
CPuttyTerminalView::~CPuttyTerminalView() {
    if ( iContainer ) {
        iAppUi->TerminalDeleted();
        AppUi()->RemoveFromStack(iContainer);
        delete iContainer;
        iContainer = NULL;
    }
}


// Second-phase constructor
void CPuttyTerminalView::ConstructL() {
    BaseConstructL(R_PUTTY_TERMINAL_VIEW);
}


// Returns a pointer to the terminal
CTerminalControl *CPuttyTerminalView::Terminal() {
    return iContainer->Terminal();
}


// Sets the font to use
void CPuttyTerminalView::SetFontL(const TDesC &aFontFile) {
    iContainer->SetFontL(aFontFile);
}


// Sets the full screen mode on/off
void CPuttyTerminalView::SetFullScreenL(TBool aFullScreen) {
    iContainer->SetFullScreenL(aFullScreen);
}


// Set terminal grayed status
void CPuttyTerminalView::SetTerminalGrayed(TBool aGrayed) {
    iContainer->SetTerminalGrayed(aGrayed);
}


// Returns the view ID
TUid CPuttyTerminalView::Id() const {
    return TUid::Uid(KPuttyTerminalViewUid);
}


// Handles a command from the system
void CPuttyTerminalView::HandleCommandL(TInt aCommand) {
    AppUi()->HandleCommandL(aCommand);
}


// Dynamically initialize a menu pane
void CPuttyTerminalView::DynInitMenuPaneL(TInt aResourceId,
                                          CEikMenuPane *aMenuPane) {
    iAppUi->DoDynInitMenuPaneL(aResourceId, aMenuPane);
}

// View activated
void CPuttyTerminalView::DoActivateL(const TVwsViewId &/*aPrevViewId*/,
                                     TUid /*aCustomMessageId*/,
                                     const TDesC8 &/*aCustomMessage*/) {
    if ( !iContainer ) {
        iContainer = CPuttyTerminalContainer::NewL(ClientRect(),
                                                   iTerminalObserver, this,
                                                   iFontFile);
        iContainer->SetMopParent(this);
        iContainer->ActivateL();
        AppUi()->AddToStackL(iContainer);
        iAppUi->TerminalCreatedL();
    }
}


// View deactivated
void CPuttyTerminalView::DoDeactivate() {
    if ( iContainer ) {
        AppUi()->RemoveFromStack(iContainer);
    }
}
