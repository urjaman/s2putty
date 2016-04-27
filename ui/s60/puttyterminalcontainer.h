/*    puttyterminalcontainer.h
 *
 * Putty UI container class for the terminal view
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2004 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __PUTTYTERMINALCONTAINER_H__
#define __PUTTYTERMINALCONTAINER_H__

#include <coecntrl.h>
#include "terminalcontrols2font.h"

static const TInt KMaxFontName = 32;

class CPuttyTerminalView;
class CS2Font;


/**
 * PuTTY UI view class for the terminal view. Owns the terminal
 * control.
 */
class CPuttyTerminalContainer : public CCoeControl {
    
public:
    /** 
     * Factory method.
     *
     * @param aRect Initial terminal container screen rectangle
     * @param aTerminalObserver Terminal observer for the terminal
     * @param aView The view that owns this container
     * @param aFontFile The font to use
     * 
     * @return A new CPuttyTerminalContainer instance.
     */
    static CPuttyTerminalContainer *NewL(const TRect &aRect,
                                         MTerminalObserver *aTerminalObserver,
                                         CPuttyTerminalView *aView,
                                         const TDesC &aFontFile);

    /** 
     * Destructor.
     */
    ~CPuttyTerminalContainer();

    
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


public: // From CCoeControl
    virtual TInt CountComponentControls() const;
    virtual CCoeControl *ComponentControl(TInt aIndex) const;
    virtual TKeyResponse OfferKeyEventL(const TKeyEvent &aKeyEvent,
                                        TEventCode aType);
    virtual void Draw(const TRect &aRect) const;
    virtual void SizeChanged();
    virtual TCoeInputCapabilities InputCapabilities() const;
    
private:
    // Constructor
    CPuttyTerminalContainer(CPuttyTerminalView *aView);

    // Second-phase Constructor
    void ConstructL(const TRect &aRect, MTerminalObserver *aTerminalObserver,
                    const TDesC &aFontFile);
    
    // Calculate a new terminal size with the current configuration
    // (font, window etc)
    void GetTerminalRect(TRect &aRect);

    CTerminalControlS2Font *iTerminal;
    TRect iTermRect;
    TBool iLargeFont;
    TBool iFullScreen;
/*     TBuf<KMaxFontName> iFontName; */
    CS2Font *iFont;
    CPuttyTerminalView *iView;
};

#endif
