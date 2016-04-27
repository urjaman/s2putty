/** @file puttyengineimp.h
 *
 * PuTTY engine implementation class
 *
 * Copyright 2002 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef __PUTTYENGINEIMP_H__
#define __PUTTYENGINEIMP_H__

#include <e32std.h>
#include <stdarg.h>
#include <gdi.h>
extern "C" {
#include "putty.h"
}
#include "puttyengine.h"
#include "puttyclient.h"
#include "epocnet.h"

/**
 * PuTTY engine implementation class. Takes care of implementing the
 * CPuttyEngine interface on top of the core PuTTY software.
 * @see CPuttyEngine
 */
class CPuttyEngineImp : public CPuttyEngine, public MSocketWatcher {

public:
    CPuttyEngineImp();
    void ConstructL(MPuttyClient *aClient, const TDesC &aDataPath);
    ~CPuttyEngineImp();

    // Methods corresponding to PuTTY callbacks
    void putty_fatalbox(char *p, va_list ap);
    void putty_connection_fatal(char *p, va_list ap);
    void putty_do_text(int x, int y, wchar_t *text, int len,
                       unsigned long attr, int lattr);
    void putty_verify_ssh_host_key(char *host, int port, char *keytype,
                                   char *keystr, char *fingerprint);
    void putty_askcipher(char *ciphername, int cs);
    void putty_palette_set(int n, int r, int g, int b);
    void putty_palette_reset();
    void putty_do_cursor(int x, int y, wchar_t *text, int len,
                         unsigned long attr, int lattr);
    int putty_from_backend(int is_stderr, const char *data, int len);
    void putty_logevent(const char *msg);
    int putty_ssh_get_line(const char *prompt, char *str, int maxlen,
                           int is_pw);

    // CPuttyEngine methods
    virtual Config *GetConfig();
    virtual TInt Connect();
    virtual void GetErrorMessage(TDes &aTarget);
    virtual void Disconnect();
    virtual void SetTerminalSize(TInt aWidth, TInt aHeight);
    virtual void RePaintWindow();
    virtual void SendKeypress(TKeyCode aCode, TUint aModifiers);
    virtual void AddRandomNoise(const TDesC8& aNoise);
    virtual void ReadConfigFileL(const TDesC &aFile);
    virtual void WriteConfigFileL(const TDesC &aFile);
    virtual void SetDefaults();

    // MSocketWatcher methods
    virtual void SocketOpened();
    virtual void SocketClosed();

    // Terminal update callback
    static TInt UpdateCallback(TAny *aAny);
    TInt UpdateTerminal();

private:
    virtual void RunL();
    virtual void DoCancel();

    enum {
        EStateNone = 0,
        EStateInitialized,
        EStateConnected,
        EStateDisconnected,
        EStateFatalConnectionError
    } iState;

    MPuttyClient *iClient;
    char *iConnError;
    TInt iNumSockets;
    TInt iTermWidth, iTermHeight;
    CPeriodic *iTermUpdatePeriodic;

    enum {
        KNumColors = 24
    };

    TRgb iDefaultPalette[KNumColors];
    TRgb iPalette[KNumColors];

    Config iConfig;
    void *iBackendHandle;
    const Backend *iBackend;
    Terminal *iTerminal;
    void *iLineDisc;
    struct unicode_data iUnicodeData;
    void *iLogContext;

    TUint16 *iTextBuf;
    TInt iTextBufLen;

    Statics iStatics;
};


#endif
