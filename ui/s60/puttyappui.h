/*    puttyappui.h
 *
 * Putty UI Application UI class
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2003 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __PUTTYAPPUI_H__
#define __PUTTYAPPUI_H__

#include <aknviewappui.h>
#include <aknprogressdialog.h>
#include "puttyclient.h"
#include "terminalcontrol.h"
#include "dialer.h"
#ifndef PUTTY_NO_AUDIORECORDER
#include "audiorecorder.h"
#endif
extern "C" {
#include "putty.h" // struct Config
}

class CPuttyTerminalView;
class CPuttyEngine;
class CEikMenuPane;
class CAknWaitDialog;


/**
 * PuTTY UI Application UI class. Contains most of the UI logic, including
 * engine and terminal callbacks.
 */
class CPuttyAppUi: public CAknViewAppUi, public MPuttyClient,
                   public MTerminalObserver, public MDialObserver,
#ifndef PUTTY_NO_AUDIORECORDER
                   public MRecorderObserver,
#endif
                   public MProgressDialogCallback {
    
public:
    void ConstructL();
    CPuttyAppUi();
    ~CPuttyAppUi();

    virtual TBool ProcessCommandParametersL(TApaCommand aCommand,
                                            TFileName &aDocumentName,
                                            const TDesC8 &aTail);
    void DoDynInitMenuPaneL(TInt aResourceId, CEikMenuPane *aMenuPane);

    /** 
     * Callback from the terminal view when the terminal control has been
     * created.
     */
    void TerminalCreatedL();

    /** 
     * Callback from the terminal view when the terminal control has been
     * created.
     */
    void TerminalDeleted();


    // MDialObserver methods
    virtual void DialCompletedL(TInt aError);

#ifndef PUTTY_NO_AUDIORECORDER
    // MRecorderObserver methods
    virtual void RecordCompleted(TInt anError);
#endif
    
    // MPuttyClient methods
    virtual void DrawText(TInt aX, TInt aY, const TDesC &aText, TBool aBold,
                          TBool aUnderline, TRgb aForeground,
                          TRgb aBackground);
    virtual void SetCursor(TInt aX, TInt aY);
    virtual void ConnectionError(const TDesC &aMessage);
    virtual void FatalError(const TDesC &aMessage);
    virtual void ConnectionClosed();
    virtual THostKeyResponse UnknownHostKey(const TDesC &aFingerprint);
    virtual THostKeyResponse DifferentHostKey(const TDesC &aFingerprint);
    virtual TBool AcceptCipher(const TDesC &aCipherName,
                               TCipherDirection aDirection);
    virtual TBool AuthenticationPrompt(const TDesC &aPrompt, TDes &aTarget,
                                       TBool aSecret);

    // MTerminalObserver methods
    virtual void TerminalSizeChanged(TInt aWidth, TInt aHeight);
    virtual void KeyPressed(TKeyCode aCode, TUint aModifiers);
    virtual void RePaintWindow();

    // MProgressDialogCallback methods
    virtual void DialogDismissedL(TInt aButtonId);

    void HandleCommandL(TInt aCommand);
    CPuttyEngine* Engine();

private:        
    virtual TKeyResponse HandleKeyEventL(const TKeyEvent& aKeyEvent,
                                         TEventCode aType);
    void ConnectionErrorL(const TDesC &aMessage);
    void FatalErrorL(const TDesC &aMessage);
    THostKeyResponse HostKeyDialogL(const TDesC &aFingerprint,
                                    TInt aDialogFormatRes);
    TBool AcceptCipherL(const TDesC &aCipherName,
                        TCipherDirection aDirection);
    void ReadUiSettingsL(Config *aConfig);
    void WriteUiSettingsL(Config *aConfig);
    TBool AuthenticationPromptL(const TDesC &aPrompt, TDes &aTarget,
                                TBool aSecret);
    
    void StringToDes(const char *aStr, TDes &aTarget);
    void DesToString(const TDesC &aDes, char *aTarget, int targetLen);

    void ShowDialWaitDialogL();
    void RemoveDialWaitDialogL();
    void ShowRecordWaitDialogL();
    void RemoveRecordWaitDialogL();
    
private:
    CPuttyTerminalView *iTerminalView;
    CPuttyEngine *iEngine;
    TInt iTermWidth, iTermHeight;
    HBufC *iFatalErrorPanic;
    TBool iFullScreen;
    CDialer *iDialer;
#ifndef PUTTY_NO_AUDIORECORDER
    CAudioRecorder *iRecorder;
    HBufC8 *iAudio;
    TPtr8 iAudioRecordDes;
    TBool iRecording;
#endif
    TFileName iDataPath;
    TFileName iFontPath;
    TFileName iFontName;
    TInt iLastCommand;
    TBool iDialWaitDialogOpen;
    HBufC *iConnectedMsg;
    CTerminalControl *iTerminal;
    TBool iRandomExists;
    HBufC **iFonts;
    TInt iNumFonts;

    enum {
        EPuttyUIStateNone = 0,
        EPuttyUIStateDialing,
        EPuttyUIStateConnecting,
        EPuttyUIStateConnected,
        EPuttyUIStateDisconnected
    } iState;

};


#endif
