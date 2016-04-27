/*    terminals2font.cpp
 *
 * A terminal UI control using a CS2Font bitmap font
 *
 * Copyright 2002,2004-2005 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <gdi.h>
#include <eikenv.h>
#include <e32svr.h>
#include "terminalcontrols2font.h"
#include "s2font.h"
#include "oneshottimer.h"
#include "logfile.h"

#define KSelectCursorXor TRgb(0x00ffff00)
const TInt KUpdateTimerDelay = 10000;

_LIT(KTerminalControlS2Font, "tcs2");
#ifdef LOGFILE_ENABLED
static void AssertFail(TInt aLine) {
    LFPRINT((_L("ASSERT FAIL: terminalcontrols2font.cpp %d"), aLine));
    User::Panic(KTerminalControlS2Font, aLine);
}
#define assert(x) __ASSERT_ALWAYS(x, AssertFail(__LINE__))
#else
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KTerminalControlS2Font, __LINE__))
#endif

#define TRACE LFPRINT((_L("terminalcontrols2font.cpp %d"), __LINE__))


// Factory method
CTerminalControlS2Font *CTerminalControlS2Font::NewL(
    MTerminalObserver &aObserver, const TRect &aRect,
    RWindow &aContainerWindow, CS2Font &aFont) {

    CTerminalControlS2Font *self =
        new (ELeave) CTerminalControlS2Font(aObserver, aFont);
    CleanupStack::PushL(self);
    self->ConstructL(aRect, aContainerWindow);
    CleanupStack::Pop(self);
    return self;
}


// Constructor
CTerminalControlS2Font::CTerminalControlS2Font(
    MTerminalObserver &aObserver, CS2Font &aFont)
    : CTerminalControl(aObserver),
      iFont(&aFont) {
    iFontHeight = iFont->FontSize().iHeight;
    iFontWidth = iFont->FontSize().iWidth;
}


// Second-phase constructor
void CTerminalControlS2Font::ConstructL(const TRect &aRect,
                                        RWindow &aContainerWindow) {
    iTimer = COneShotTimer::NewL(TCallBack(UpdateCallBack, this));
    CTerminalControl::ConstructL(aRect, aContainerWindow);    
}


// Destructor
CTerminalControlS2Font::~CTerminalControlS2Font() {
    delete [] iDirtyLeft;
    delete [] iDirtyRight;
    delete iTimer;
    delete iRowBitmapGc;
    delete iRowBitmapDevice;
    delete iRowBitmap;
    delete iRowTextBuf;
}


// Set font
void CTerminalControlS2Font::SetFontL(CS2Font &aFont) {

    iFont = &aFont;
    iFontHeight = iFont->FontSize().iHeight;
    iFontWidth = iFont->FontSize().iWidth;
    Resize();
}


// (re)allocate buffers
void CTerminalControlS2Font::AllocateBuffersL() {
    delete [] iDirtyLeft;
    iDirtyLeft = NULL;
    delete [] iDirtyRight;
    iDirtyRight = NULL;
    delete iRowBitmapGc;
    iRowBitmapGc = NULL;
    delete iRowBitmapDevice;
    iRowBitmapDevice = NULL;
    delete iRowBitmap;
    iRowBitmap = NULL;
    delete iRowTextBuf;
    iRowTextBuf = NULL;

    iRowBitmap = new CFbsBitmap();
    TDisplayMode displayMode = SystemGc().Device()->DisplayMode();
    if ( (displayMode != EColor4K) && (displayMode != EColor64K) ) {
        displayMode = EColor64K;
    }
    User::LeaveIfError(iRowBitmap->Create(TSize(iCharWidth*iFontWidth,
                                                iFontHeight),
                                          displayMode));
    iRowBitmapDevice = CFbsBitmapDevice::NewL(iRowBitmap);
    iRowBitmapGc = CFbsBitGc::NewL();
    iDirtyLeft = new (ELeave) TInt[iCharHeight];
    iDirtyRight = new (ELeave) TInt[iCharHeight];
    iRowTextBuf = HBufC::NewL(iCharWidth);
    
    CTerminalControl::AllocateBuffersL();
}


// Clear buffers
void CTerminalControlS2Font::Clear() {
    for ( TInt y = 0; y < iCharHeight; y++ ) {
        iDirtyLeft[y] = iCharWidth;
        iDirtyRight[y] = -1;
    }
    CTerminalControl::Clear();
}


// Updates one more characters on a line to the display
void CTerminalControlS2Font::UpdateDisplay(TInt aX, TInt aY, TInt aLength) {

    assert((aX >= 0) && ((aX + aLength) <= iCharWidth));
    assert((aY >= 0) && (aY < iCharHeight));
    
    // Mark the area dirty
    if ( iDirtyLeft[aY] > aX ) {
        iDirtyLeft[aY] = aX;
    }
    if ( iDirtyRight[aY] < (aX + aLength - 1) ) {
        iDirtyRight[aY] = aX + aLength - 1;
    }
    
    StartUpdateTimer();
}


// Updates one more characters on a line to the display.
// This method requires a valid graphics context and window server session
void CTerminalControlS2Font::UpdateWithGc(CBitmapContext &aGc,
                                          RWsSession &aWs,
                                          TInt aX, TInt aY,
                                          TInt aLength) const {
    
    assert((aX >= 0) && ((aX + aLength) <= iCharWidth));
    assert((aY >= 0) && (aY < iCharHeight));

    LFPRINT((_L("UpdateWithGc(gc, ws, %d, %d, %d)"), aX, aY, aLength));

    TInt x0 = Rect().iTl.iX;
    TInt y0 = Rect().iTl.iY;
    
    // We'll first render all runs of identical updates at one go to the
    // bitmap, render the cursor if necessary, and finally blit the bitmap
    // to the screen    

    TTerminalAttribute *attribs = &iAttributes[aY * iCharWidth + aX];
    TInt x = 0;

    while ( x < aLength ) {
        // Initial attributes and colors
        TTerminalAttribute &a0 = attribs[x];
        TRgb fg0, bg0;
        GetFinalColors(aX+x, aY, fg0, bg0);

        // Attributes and colors for this character
        TTerminalAttribute *a = &attribs[x];
        TRgb fg, bg;
        GetFinalColors(aX+x, aY, fg, bg);

        TInt num = 0;
        do {
            num++;
            if ( (x+num) < aLength ) {
                a++;
                GetFinalColors(aX+x+num, aY, fg, bg);
                if ( (fg != fg0) || (bg != bg0) ||
                     (a->iBold != a0.iBold) ||
                     (a->iUnderline != a0.iUnderline) ) {
                    break;
                }
            }
        } while ( (x+num) < aLength );

        TPtr text = iRowTextBuf->Des();
        text.Zero();
        for ( TInt i = 0; i < num; i++ ) {
            TText c = FinalChar(aX+x+i, aY);
            // Our fonts contain nothing at the line break character, map
            // it to "CR" since we don't really have better alternatives
            if ( c == 0x2028 ) {
                c = 0x240d;
            }
            text.Append(c);
        }

        iFont->RenderText(*iRowBitmap, text, (aX+x) * iFontWidth, 0, fg0, bg0);
        x += num;
    }
    
    // Draw the selection cursor to the bitmap if necessary
    if ( iSelectMode && ((iSelectY == aY) &&
                         (iSelectX >= aX) && (iSelectX < (aX + aLength))) ) {

        iRowBitmapGc->Activate(iRowBitmapDevice);
        iRowBitmapGc->Reset();
        iRowBitmapGc->SetBrushStyle(CGraphicsContext::ENullBrush);
        iRowBitmapGc->SetDrawMode(CGraphicsContext::EDrawModeXOR);
        iRowBitmapGc->SetPenStyle(CGraphicsContext::ESolidPen);
        iRowBitmapGc->SetPenColor(KSelectCursorXor);
        iRowBitmapGc->DrawRect(TRect(TPoint(iSelectX * iFontWidth, 0),
                                     TSize(iFontWidth, iFontHeight)));
    }
    
    // Blit
    aGc.BitBlt(TPoint(x0 + aX*iFontWidth, y0 + aY*iFontHeight),
              iRowBitmap,
              TRect(aX*iFontWidth, 0, (aX+aLength)*iFontWidth, iFontHeight));
    aWs.Flush();
    TRACE;
}


// CCoeControl::Draw()
void CTerminalControlS2Font::Draw(const TRect & /*aRect*/) const {

    CWindowGc &gc = SystemGc();
    gc.Reset();
    RWsSession &ws = CCoeEnv::Static()->WsSession();    

    if ( iGrayed ) {
        gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
        gc.SetBrushColor(TRgb(0xcccccc));
        gc.SetPenStyle(CGraphicsContext::ENullPen);
        gc.DrawRect(Rect());
    }
    else {
        // Draw everything and mark all lines not dirty
        for ( TInt y = 0; y < iCharHeight; y++ ) {
            UpdateWithGc(gc, ws, 0, y, iCharWidth);
            iDirtyLeft[y] = iCharWidth;
            iDirtyRight[y] = -1;
        }
    }
}


// Start update timer
void CTerminalControlS2Font::StartUpdateTimer() {
    // We don't want to restart the timer for every update, just use it to
    // delay updates a bit so that we don't need to do too many small
    // updates
    if ( !iTimerRunning ) {
        iTimer->After(KUpdateTimerDelay);
        iTimerRunning = ETrue;
    }
}


// Update all dirty areas on the screen
void CTerminalControlS2Font::Update() {

    iTimerRunning = EFalse;

    CWindowGc &gc = SystemGc();
    gc.Activate(Window());
    RWsSession &ws = CCoeEnv::Static()->WsSession();
    
    // Go through each line, finding areas that need updating. On those lines
    // we'll first render all runs of identical updates at one go to the
    // bitmap, render the cursor if necessary, and finally blit the bitmap
    // to the screen
    for ( TInt y = 0; y < iCharHeight; y++ ) {

        // Skip lines that are fully up to date
        if ( (iDirtyRight[y] < iDirtyLeft[y]) ) {
            continue;
        }

        // Draw the line to the display
        UpdateWithGc(gc, ws, iDirtyLeft[y], y, iDirtyRight[y]-iDirtyLeft[y]+1);

        // No longer dirty
        iDirtyLeft[y] = iCharWidth;
        iDirtyRight[y] = -1;
    }

    gc.Deactivate();
}


// Update timer callback
TInt CTerminalControlS2Font::UpdateCallBack(TAny *aPtr) {
    ((CTerminalControlS2Font*)aPtr)->Update();
    return 0;
}
