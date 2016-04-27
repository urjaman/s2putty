/*    puttyterminalview.h
 *
 * Putty UI View class for the terminal view
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002-2005 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __PUTTYTERMINALVIEW_H__
#define __PUTTYTERMINALVIEW_H__

#include <aknview.h>
#include "terminalcontrol.h"

static const TInt KPuttyTerminalViewUid = 0x101f9079;

class CPuttyTerminalContainer;
class CPuttyAppUi;


/**
 * PuTTY UI view class for the terminal view.
 */
class CPuttyTerminalView : public CAknView {

    friend class CPuttyTerminalContainer;

public:
    /** 
     * Factory method.
     * 
     * @param aTerminalObserver Terminal observer for the terminal
     * @param aAppUi The application UI object
     * @param aFontFile The font to use
     * 
     * @return A new CPuttyTerminalView instance.
     */
    static CPuttyTerminalView *NewL(MTerminalObserver *aTerminalObserver,
                                    CPuttyAppUi *aAppUi,
                                    const TDesC &aFontFile);

    /** 
     * Destructor.
     */
    ~CPuttyTerminalView();

    
    /** 
     * Returns a pointer to the terminal control.
     * 
     * @return The terminal control in use.
     */
    CTerminalControl *Terminal();

    /** 
     * Sets the font to use.
     * 
     * @param aFontFile Font file name
     */
    void SetFontL(const TDesC &aFontFile);
    
    /** 
     * Sets full screen mode on/off
     * 
     * @param aFullScreen ETrue to use full screen mode
     */
    void SetFullScreenL(TBool aFullScreen);

    /** 
     * Set terminal grayed status
     * 
     * @param aGrayed ETrue to gray terminal out, EFalse to activate it
     */
    void SetTerminalGrayed(TBool aGrayed);

    
public: // from CAknView
    /** 
     * Returns the view ID
     * 
     * @return View ID
     */
    TUid Id() const;

    /** 
     * System command handle callback
     * 
     * @param aCommand The command to be handled
     */
    void HandleCommandL(TInt aCommand);

    /** 
     * Dynamically initialize a menu pane
     * 
     * @param aResourceId Menu resource ID
     * @param aMenuPane Menu pane object
     * 
     * @return 
     */
    void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane);

    /** 
     * Called by the system when the view is activated.
     */
    void DoActivateL(const TVwsViewId& aPrevViewId,
                     TUid aCustomMessageId,
                     const TDesC8& aCustomMessage);
    
    /** 
     * Called by the system when the view is deactivated.
     */
    void DoDeactivate();
    

private:
    // Constructor
    CPuttyTerminalView(MTerminalObserver *aTerminalObserver,
                       CPuttyAppUi *aAppUi,
                       const TDesC &aFontFile);

    // Second-phase constructor
    void ConstructL();


private: // Data
    CPuttyTerminalContainer *iContainer;
    CPuttyAppUi *iAppUi;
    MTerminalObserver *iTerminalObserver;
    TFileName iFontFile;
};


#endif
