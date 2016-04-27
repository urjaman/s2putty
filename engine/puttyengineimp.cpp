/*    puttyengineimp.cpp
 *
 * PuTTY engine implementation class
 *
 * Copyright 2002,2003,2005 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <assert.h>
#include <stdlib.h>
#include "charutil.h"
#include "puttyengineimp.h"
#include "epocmemory.h"
#include "epocnoise.h"
#include "epocnet.h"
#include "epocstore.h"
#include "terminalkeys.h"
extern "C" {
#include "storage.h"
#include "ssh.h"
}


_LIT(KOutOfMemory, "Out of memory");

static const unsigned char KDefaultColors[22][3] = {
    { 0,0,0 }, { 128,128,128 }, { 255,255,255 }, { 255,255,255 }, { 0,0,0 },
    { 128,128,192 }, { 0,0,0 }, { 85,85,85 }, { 187,0,0 }, { 255,85,85 },
    { 0,187,0 }, { 85,255,85 }, { 187,187,0 }, { 255,255,85 }, { 0,0,187 },
    { 85,85,255 }, { 187,0,187 }, { 255,85,255 }, { 0,187,187 },
    { 85,255,255 }, { 187,187,187 }, { 192,192,192 }
};

const struct backend_list backends[] = {
    {PROT_SSH, "ssh", &ssh_backend},
    {0, NULL}
};

static int do_ssh_get_line(const char *prompt, char *str, int maxlen,
                           int is_pw);


// Factory methods

EXPORT_C CPuttyEngine *CPuttyEngine::NewL(MPuttyClient *aClient,
                                          const TDesC &aDataPath) {
    CPuttyEngineImp *self = new (ELeave) CPuttyEngineImp;
    CleanupStack::PushL(self);
    self->ConstructL(aClient, aDataPath);
    CleanupStack::Pop();
    return self;
}

// Two-phase construction
CPuttyEngineImp::CPuttyEngineImp() {
    
    set_statics_tls(&iStatics);
    Mem::FillZ(statics(), sizeof(Statics));
    statics()->frontend = this;
    statics()->ssh_get_line = do_ssh_get_line;
    iState = EStateNone;
    iTermWidth = 80;
    iTermHeight = 24;
}

void CPuttyEngineImp::ConstructL(MPuttyClient *aClient,
                                 const TDesC &aDataPath) {
    // Initialize Symbian OS statics
    SymbianStatics *sstats =
        (SymbianStatics*) User::AllocL(sizeof(SymbianStatics));
    Mem::FillZ(sstats, sizeof(SymbianStatics));
    statics()->platform = sstats;
    
    iClient = aClient;
    iConnError = NULL;
    iNumSockets = 0;
    
    // Initialize Symbian-port bits
    epoc_memory_init();
    epoc_store_init(aDataPath);
    epoc_noise_init();
    random_init();

    SetDefaults();

    iState = EStateInitialized;
    
    CActiveScheduler::Add(this);
}


// Destruction
CPuttyEngineImp::~CPuttyEngineImp() {
    
    if ( (iState == EStateConnected) || (iState == EStateDisconnected) ) {
        Disconnect();
    } else {
        assert(iState == EStateInitialized);
    }

    // Save random number generator seed. The Symbian OS implementation won't
    // actually save it unless the generator has been seeded properly, so this
    // can be called unconditionally.
    random_save_seed();

    random_free();
    
    sfree(iTextBuf);
    
    // Uninitialize Symbian stuff
    epoc_noise_free();
    epoc_store_free();
    epoc_memory_free();

    CloseSTDLIB();

    iState = EStateNone;    
    Cancel();
    
    delete iTermUpdatePeriodic;

    // Free static variables
    User::Free(statics()->platform);
    statics()->platform = NULL;
    statics()->frontend = NULL;
    remove_statics_tls();

    delete[] iConnError;
}


// MPuttyEngine::GetConfig()
Config *CPuttyEngineImp::GetConfig() {
    return &iConfig;
}


// MPuttyEngine::Connect()
TInt CPuttyEngineImp::Connect() {

    assert(iState == EStateInitialized);

    // Initialize logging
    iLogContext = log_init(this, &iConfig); 

    // Convert palette from the config to our internal palette
    static const TInt palettemap[24] = {
	6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 0, 1, 2, 3, 4, 4, 5, 5
    };
    for ( TInt c = 0; c < 24; c++ ) {
        TInt idx = palettemap[c];
        iDefaultPalette[c].SetRed(iConfig.colours[idx][0]);
        iDefaultPalette[c].SetGreen(iConfig.colours[idx][1]);
        iDefaultPalette[c].SetBlue(iConfig.colours[idx][2]);
    }
    putty_palette_reset();
    
    // Initialize charset conversion tables
    init_ucs(&iUnicodeData, iConfig.line_codepage, iConfig.vtmode);

    // Select the protocol to use
    iBackend = NULL;
    for ( TInt i = 0; backends[i].backend != NULL; i++ ) {
        if ( backends[i].protocol == iConfig.protocol ) {
            iBackend = backends[i].backend;
            break;
        }
    }
    if ( iBackend == NULL ) {
        fatalbox("Unsupported backend");
    }

    // Initialize networking
    sk_init();
    iNumSockets = 0;
    sk_set_watcher(this);
    sk_provide_logctx(iLogContext);

    // Connect
    char *realhost;
    delete [] iConnError;
    iConnError = NULL;
    const char *err = iBackend->init(this, &iBackendHandle, &iConfig,
                                     iConfig.host, iConfig.port, &realhost,
                                     iConfig.tcp_nodelay,
                                     iConfig.tcp_keepalives);
    if ( err ) {
        iConnError = new char[strlen(err)+1];
        if ( !iConnError ) {
            return KErrNoMemory;
        }
        strcpy(iConnError, err);
        sk_set_watcher(NULL);
        sk_cleanup();
        if ( iLogContext ) {
            logflush(iLogContext);
        }
        return KErrGeneral;
    }
    sfree(realhost);
    iBackend->provide_logctx(iBackendHandle, iLogContext);

    // Initialize terminal
    iTerminal = term_init(&iConfig, &iUnicodeData, this);
    term_provide_logctx(iTerminal, iLogContext);
    term_size(iTerminal, iTermHeight, iTermWidth, iConfig.savelines);

    // Connect the terminal to the backend for resize purposes.
    term_provide_resize_fn(iTerminal, iBackend->size, iBackendHandle);

    // Set up a line discipline.
    iLineDisc = ldisc_create(&iConfig, iTerminal, iBackend, iBackendHandle,
                             this);

    // Start a periodic timer to update the terminal (FIXME?)
    delete iTermUpdatePeriodic;
    iTermUpdatePeriodic = NULL;
    iTermUpdatePeriodic = CPeriodic::NewL(EPriorityNormal);
    iTermUpdatePeriodic->Start(25000, 25000, TCallBack(UpdateCallback, this));

    iState = EStateConnected;

    return KErrNone;
}


// MPuttyEngine::GetErrorMessage()
void CPuttyEngineImp::GetErrorMessage(TDes &aTarget) {

    if ( iConnError == NULL ) {
        aTarget.SetLength(0);
        return;
    }

    StringToDes(iConnError, aTarget);
}


// MPuttyEngine::Disconnect()
void CPuttyEngineImp::Disconnect() {

    // ARGH! FIXME! This won't really work if the connection is still open.
    // There is really no way to close a PuTTY connection forcibly except by
    // terminating the whole thing!
    assert((iState == EStateConnected) || (iState == EStateDisconnected));

    // Get rid of the timer
    delete iTermUpdatePeriodic;
    iTermUpdatePeriodic = NULL;

    // Close networking
    sk_set_watcher(NULL);
    sk_cleanup();

    term_free(iTerminal);
    iTerminal = NULL;
    ldisc_free(iLineDisc);
    iLineDisc = NULL;
    log_free(iLogContext);
    iLogContext = NULL;

    iState = EStateInitialized;
    delete[] iConnError;
    iConnError = NULL;
}


// MPuttyEngine::SetTerminalSize();
void CPuttyEngineImp::SetTerminalSize(TInt aWidth, TInt aHeight) {
    iTermWidth = aWidth;
    iTermHeight = aHeight;
    assert((iTermWidth > 1) && (iTermHeight > 1));

    if ( iState == EStateConnected ) {
        term_size(iTerminal, iTermHeight, iTermWidth, iConfig.savelines);
    }
}


// MPuttyEngine::RePaintWindow()
void CPuttyEngineImp::RePaintWindow() {
    if ( iState != EStateConnected ) {
        return;
    }

    term_paint(iTerminal, (Context)this, 0, 0, iTermWidth-1, iTermHeight-1, 1);
}


// MPuttyEngine::SendKeypress
void CPuttyEngineImp::SendKeypress(TKeyCode aCode, TUint aModifiers) {
    if ( iState != EStateConnected ) {
        return;
    }

    char buf[16];

    // Reset scrollback
    term_seen_key_event(iTerminal);

    // Interrupt paste
    term_nopaste(iTerminal);

    // Try to translate as a control character
    int xbytes = TranslateKey(&iConfig, iTerminal, aCode, aModifiers, buf);
    assert(xbytes < 16);
    if ( xbytes > 0 ) {
        // Send control key codes as is
        ldisc_send(iLineDisc, buf, xbytes, 1);
        
    } else if ( xbytes == -1 ) {
        // Not translated, probably a normal letter
        if ( aCode < ENonCharacterKeyBase ) {
            wchar_t wc = (wchar_t) aCode;
            luni_send(iLineDisc, &wc, 1, 1);
        }
    }
}


// MPuttyEngine::AddRandomNoise
void CPuttyEngineImp::AddRandomNoise(const TDesC8& aNoise) {
    assert(iState != EStateNone);
    random_add_noise((void*)aNoise.Ptr(), aNoise.Length());
}


// MPuttyEngine::ReadConfigFileL
void CPuttyEngineImp::ReadConfigFileL(const TDesC &aFile) {
    assert(iState != EStateNone);
    char *name = new (ELeave) char[aFile.Length()+1];
    DesToString(aFile, name);
    do_defaults(name, &iConfig);
    delete [] name;
}


// MPuttyEngine::WriteConfigFileL
void CPuttyEngineImp::WriteConfigFileL(const TDesC &aFile) {
    assert(iState != EStateNone);
    char *name = new (ELeave) char[aFile.Length()+1];
    DesToString(aFile, name);
    save_settings(name, TRUE, &iConfig);
    delete [] name;
}


// MPuttyEngine::SetDefaults
void CPuttyEngineImp::SetDefaults() {
    
    statics()->flags = FLAG_INTERACTIVE;
    statics()->default_protocol = PROT_SSH;
    statics()->default_port = 22;    
    do_defaults(NULL, &iConfig);
    statics()->default_protocol = iConfig.protocol;
    statics()->default_port = iConfig.port;
    strcpy(iConfig.line_codepage, "ISO-8859-15");
    iConfig.vtmode = VT_UNICODE;
    iConfig.sshprot = 1;
    iConfig.compression = 1;
    for ( TInt i = 0; i < 22; i++ ) {
        iConfig.colours[i][0] = KDefaultColors[i][0];
        iConfig.colours[i][1] = KDefaultColors[i][1];
        iConfig.colours[i][2] = KDefaultColors[i][2];
    }
    strcpy(iConfig.logfilename.path, "c:\\putty.log");
}


// MSocketWatcher::SocketOpened()
void CPuttyEngineImp::SocketOpened() {
    iNumSockets++;
}


// MSocketWatcher::SocketClosed()
void CPuttyEngineImp::SocketClosed() {
    iNumSockets--;
    if ( (iState == EStateConnected) && (iNumSockets == 0) ) {
        // Disconnect, signal ourselves
        iState = EStateDisconnected;
        SetActive();
        TRequestStatus *status = &iStatus;
        User::RequestComplete(status, KErrNone);
    }
}


// Fatal error (PuTTY callback)
void CPuttyEngineImp::putty_fatalbox(char *p, va_list ap) {

    if ( iLogContext ) {
        logflush(iLogContext);
    }
    char *buf = (char*) smalloc(1024);
    HBufC *des = HBufC::New(1024);
    if ( (buf == NULL) || (des == NULL) ) {
        iClient->FatalError(KOutOfMemory);
    }

    vsnprintf(buf, 1023, p, ap);
    TPtr ptr = des->Des();
    StringToDes(buf, ptr);
    iClient->FatalError(ptr);
    delete des;
    sfree(buf);
}

void fatalbox(char *p, ...) {
    CPuttyEngineImp *engine = (CPuttyEngineImp*) statics()->frontend;
    assert(engine);
    va_list ap;
    va_start(ap, p);
    engine->putty_fatalbox(p, ap);
    va_end(ap);
}


// Fatal connection error (PuTTY callback)
void CPuttyEngineImp::putty_connection_fatal(char *p, va_list ap) {

    if ( iLogContext ) {
        logflush(iLogContext);
    }
    char *buf = (char*) smalloc(1024);
    HBufC *des = HBufC::New(1024);
    if ( (buf == NULL) || (des == NULL) ) {
        iClient->FatalError(KOutOfMemory);
    }

    vsnprintf(buf, 1023, p, ap);
    TPtr ptr = des->Des();
    StringToDes(buf, ptr);
    iClient->ConnectionError(ptr);
    delete des;
    sfree(buf);
}

void connection_fatal(void *frontend, char *p, ...) {
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    va_list ap;
    va_start(ap, p);
    engine->putty_connection_fatal(p, ap);
    va_end(ap);
}


// Draw text on screen
void CPuttyEngineImp::putty_do_text(int x, int y, wchar_t *text, int len,
                                    unsigned long attr, int /*lattr*/) {

    // Make sure we have a resonable character set
    assert((attr & CSET_MASK) != CSET_OEMCP);
    assert((attr & CSET_MASK) != CSET_ACP);
    assert(!DIRECT_CHAR(attr));
    assert(!DIRECT_FONT(attr));

    // With PuTTY 0.56 and later the characters are now proper wide chars,
    // so we can use them as they are

    // Handle cursor attributes
    if ( attr & TATTR_ACTCURS ) {
	attr &= ATTR_CUR_AND;
	attr ^= ATTR_CUR_XOR;
    }

    // Determine the colors to use
    TInt fgIndex = ((attr & ATTR_FGMASK) >> ATTR_FGSHIFT);
    fgIndex = 2 * (fgIndex & 0xF) + (fgIndex & 0x10 ? 1 : 0);
    TInt bgIndex = ((attr & ATTR_BGMASK) >> ATTR_BGSHIFT);
    bgIndex = 2 * (bgIndex & 0xF) + (bgIndex & 0x10 ? 1 : 0);
    if ( attr & ATTR_REVERSE ) {
        TInt tmp = fgIndex;
        fgIndex = bgIndex;
        bgIndex = tmp;
    }
    // FIXME: Bold colors
#if 0
    if ( (bold_mode == BOLD_COLOURS) && (attr & ATTR_BOLD) ) {
        fgIndex |= 1;
    }
    if ( (bold_mode == BOLD_COLOURS) && (attr & ATTR_BLINK) ) {
        bgIndex |= 1;
    }
#endif
    assert((fgIndex >= 0) && (bgIndex >= 0) &&
           (fgIndex < KNumColors) && (bgIndex < KNumColors));

    // Draw -- FIXME: Colors and attributes!
    TPtrC16 ptr((const TUint16*) text, len);
    iClient->DrawText(x, y, ptr, EFalse, EFalse, iPalette[fgIndex],
                      iPalette[bgIndex]);
}

void do_text(Context ctx, int x, int y, wchar_t *text, int len,
	     unsigned long attr, int lattr) {
    assert(ctx);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) ctx;
    engine->putty_do_text(x, y, text, len, attr, lattr);
}


// Verify SSH host key (PuTTY callback)
void CPuttyEngineImp::putty_verify_ssh_host_key(
    char *host, int port, char *keytype, char *keystr, char *fingerprint) {

    // Verify key against the store
    int keystatus = verify_host_key(host, port, keytype, keystr);

    if ( keystatus == 0 ) {
        return; // matched to a previously stored key
    }

    TPtr *fpDes = CreateDes(fingerprint);
    MPuttyClient::THostKeyResponse resp = MPuttyClient::EAbadonConnection;

    // Key not verified OK, prompt the user
    if (keystatus == 2 ) {
        // Key in store, but different
        resp = iClient->DifferentHostKey(*fpDes);
    } else if ( keystatus == 1 ) {
        // Key not in store
        resp = iClient->UnknownHostKey(*fpDes);
    } else {
        assert(EFalse);
    }

    DeleteDes(fpDes);

    // React
    switch ( resp ) {
        case MPuttyClient::EAbadonConnection:
            // FIXME! There is no other safe way to close connection _NOW, and
            // we must be sure we don't send anything to the server
            if ( iLogContext ) {
                logflush(iLogContext);
            }
            User::Exit(KErrCancel);
            break;

        case MPuttyClient::EAcceptTemporarily:
            break;

        case MPuttyClient::EAcceptAndStore:
	    store_host_key(host, port, keytype, keystr);
            break;

        default:
            assert(EFalse);
    }
}

void verify_ssh_host_key(void *frontend, char *host, int port,
                         char *keytype, char *keystr, char *fingerprint) {
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    engine->putty_verify_ssh_host_key(host, port, keytype, keystr,
                                      fingerprint);
}



// Prompt the user to accept a cipher below the warning threshold
void CPuttyEngineImp::putty_askcipher(char *ciphername, int cs) {
    
    TPtr *cipherDes = CreateDes(ciphername);
    MPuttyClient::TCipherDirection dir = MPuttyClient::EBothDirections;

    // Convert direction to client interface constant
    switch ( cs ) {
        case 0:
            dir = MPuttyClient::EBothDirections;
            break;

        case 1:
            dir = MPuttyClient::EClientToServer;
            break;

        case 2:
            dir = MPuttyClient::EServerToClient;
            break;

        default:
            assert(EFalse);
    }

    // Prompt the user, kill connection if the cipher is not acceptable
    if ( !iClient->AcceptCipher(*cipherDes, dir) ) {
        // FIXME!
        DeleteDes(cipherDes);
        if ( iLogContext ) {
            logflush(iLogContext);
        }
        User::Exit(KErrCancel);
    }

    DeleteDes(cipherDes);
}


void askcipher(void *frontend, char *ciphername, int cs) {
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    engine->putty_askcipher(ciphername, cs);
}


void old_keyfile_warning(void)
{
    // FIXME?
    fatalbox("Unsupported old private key file format");
}


// Set a palette entry (copied pretty much from window.c)
void CPuttyEngineImp::putty_palette_set(int n, int r, int g, int b) {
    
    static const int first[21] = {
	0, 2, 4, 6, 8, 10, 12, 14,
	1, 3, 5, 7, 9, 11, 13, 15,
	16, 17, 18, 20, 22
    };

    assert((n > 0) && (n < 21));
    assert((r > 0) && (r < 256));
    assert((g > 0) && (g < 256));
    assert((b > 0) && (g < 256));
    
    iPalette[first[n]].SetRed(r);
    iPalette[first[n]].SetGreen(g);
    iPalette[first[n]].SetBlue(b);

    if ( first[n] >= 18 ) {
        iPalette[first[n]+1].SetRed(r);
        iPalette[first[n]+1].SetGreen(g);
        iPalette[first[n]+1].SetBlue(b);
    }
}

void palette_set(void *frontend, int n, int r, int g, int b)
{
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    engine->putty_palette_set(n, r, g, b);
}


// Reset to the default palette
void CPuttyEngineImp::putty_palette_reset() {
    Mem::Copy(iPalette, iDefaultPalette, sizeof(iPalette));
}

void palette_reset(void *frontend)
{
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    engine->putty_palette_reset();
}


// Draw the cursor
void CPuttyEngineImp::putty_do_cursor(int x, int y,
                                      wchar_t * /*text*/, int /*len*/,
                                      unsigned long /*attr*/, int /*lattr*/) {
    iClient->SetCursor(x, y);
}

void do_cursor(Context ctx, int x, int y, wchar_t *text, int len,
	       unsigned long attr, int lattr) {
    assert(ctx);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) ctx;
    engine->putty_do_cursor(x, y, text, len, attr, lattr);
}


// Clean up and exit
void cleanup_exit(int code) {
    CPuttyEngineImp *engine = (CPuttyEngineImp*) statics()->frontend;
    delete engine;
    User::Exit(code);
}


// Data from backend
int CPuttyEngineImp::putty_from_backend(int is_stderr, const char *data,
                                        int len) {
    return term_data(iTerminal, is_stderr, data, len);
}

int from_backend(void *frontend, int is_stderr, const char *data, int len)
{
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    return engine->putty_from_backend(is_stderr, data, len);
}


// Log message
void CPuttyEngineImp::putty_logevent(const char *msg) {
    log_eventlog(iLogContext, msg);
}

void logevent(void *frontend, const char *msg) {
    assert(frontend);
    CPuttyEngineImp *engine = (CPuttyEngineImp*) frontend;
    engine->putty_logevent(msg);
}



// SSH authentication prompt
int CPuttyEngineImp::putty_ssh_get_line(const char *prompt, char *str,
                                        int maxlen, int is_pw) {
    
    HBufC *promptBuf = HBufC::New(strlen(prompt));
    if ( !promptBuf ) {
        iClient->FatalError(KOutOfMemory);
    }
    TPtr16 promptDes = promptBuf->Des();
    StringToDes(prompt, promptDes);

    assert(maxlen > 1);
    HBufC *destBuf = HBufC::New(maxlen-1);
    if ( !destBuf ) {
        iClient->FatalError(KOutOfMemory);
    }
    TPtr16 destDes = destBuf->Des();

    TBool ok = iClient->AuthenticationPrompt(promptDes, destDes,
                                             is_pw ? ETrue : EFalse);
    if ( ok ) {
        DesToString(destDes, str);
    }

    delete destBuf;
    delete promptBuf;
    return ok ? 1 : 0;
}

int do_ssh_get_line(const char *prompt, char *str, int maxlen,
                    int is_pw) {
    // FIXME: Should change ssh_get_line to include a frontend pointer
    CPuttyEngineImp *engine = (CPuttyEngineImp*) statics()->frontend;
    return engine->putty_ssh_get_line(prompt, str, maxlen, is_pw);
}



// Create and destroy a fronend context (PuTTY callbacks). Since we don't
// maintain any state in a context, the context is the frontend.
Context get_ctx(void *frontend)
{
    return (Context) frontend;
}

void free_ctx(Context /*ctx*/)
{
}



// Update terminal
TInt CPuttyEngineImp::UpdateTerminal() {
    term_out(iTerminal);
    term_update(iTerminal);
    return 0;
}

TInt CPuttyEngineImp::UpdateCallback(TAny *aAny) {
    return ((CPuttyEngineImp*)aAny)->UpdateTerminal();
}


void CPuttyEngineImp::RunL() {

    assert(iStatus.Int() == KErrNone);

    if ( iState == EStateDisconnected ) {
        Disconnect();
        iClient->ConnectionClosed();
    } else {
        assert(EFalse);
    }
}


void CPuttyEngineImp::DoCancel() {
    assert(EFalse);
}



/**********************************************************
 *
 * Other PuTTY callback functions, most of these do nothing
 *
 **********************************************************/


/*
 * Move the system caret. (We maintain one, even though it's
 * invisible, for the benefit of blind people: apparently some
 * helper software tracks the system caret, so we should arrange to
 * have one.)
 */
void sys_cursor(void * /*frontend*/, int /*x*/, int /*y*/)
{
}


void set_sbar(void * /*frontend*/, int /*total*/, int /*start*/, int /*page*/)
{
}


char *get_window_title(void * /*frontend*/, int icon)
{
    return (char*) (icon ? "PuTTY.Icon" : "PuTTY");
}


/*
 * Report the window's position, for terminal reports.
 */
void get_window_pos(void * /*frontend*/, int *x, int *y)
{
    *x = 0;
    *y = 0;
}

/*
 * Report the window's pixel size, for terminal reports.
 */
void get_window_pixels(void * /*frontend*/, int *x, int *y)
{
    /* FIXME? */
    *x = 640;
    *y = 200;
}

/*
 * Report whether the window is iconic, for terminal reports.
 */
int is_iconic(void * /*frontend*/)
{
    return 0;
}


/*
 * Maximise or restore the window in response to a server-side
 * request.
 */
void set_zoomed(void * /*frontend*/, int /*zoomed*/)
{
}

/*
 * Refresh the window in response to a server-side request.
 */
void refresh_window(void * /*frontend*/)
{
    /* FIXME? */
}


/*
 * Move the window to the top or bottom of the z-order in response
 * to a server-side request.
 */
void set_zorder(void * /*frontend*/, int /*top*/)
{
}

/*
 * Move the window in response to a server-side request.
 */
void move_window(void * /*frontend*/, int /*x*/, int /*y*/)
{
}


/*
 * Minimise or restore the window in response to a server-side
 * request.
 */
void set_iconic(void * /*frontend*/, int /*iconic*/)
{
}

void request_resize(void * /*frontend*/, int /*w*/, int /*h*/)
{
}

/*
 * Beep.
 */
void beep(void * /*frontend*/, int /*mode*/)
{
}

/*
 * set or clear the "raw mouse message" mode
 */
void set_raw_mouse_mode(void * /*frontend*/, int /*activate*/)
{
}

void set_title(void * /*frontend*/, char * /*title*/)
{
}

void set_icon(void * /*frontend*/, char * /*title*/)
{
}



/*
 * Writes stuff to clipboard
 */
void write_clip(void * /*frontend*/, wchar_t * /*data*/, int /*len*/,
                int /*must_deselect*/)
{
}

void get_clip(void * /*frontend*/, wchar_t ** /*p*/, int * /*len*/)
{
}


/*
 * Translate a raw mouse button designation (LEFT, MIDDLE, RIGHT)
 * into a cooked one (SELECT, EXTEND, PASTE).
 */
Mouse_Button translate_button(Mouse_Button /*button*/)
{
    return (Mouse_Button) 0;
}


/* This function gets the actual width of a character in the normal font.
 */
int CharWidth(Context /*ctx*/, int /*uc*/) {

    return 1;
}

extern "C" void ldisc_update(void * /*frontend*/, int /*echo*/, int /*edit*/)
{
}

char *do_select(RSocketS * /*skt*/, int /*startup*/)
{
    return NULL;
}

void frontend_keypress(void * /*handle*/)
{
    return;
}

void update_specials_menu(void * /*frontend*/) {
}

void request_paste(void * /*frontend*/)
{
    /* FIXME: term_do_paste(term); */
}

int char_width(Context /*ctx*/, int /*uc*/) {
    return 1;
}

int askappend(void * /*frontend*/, Filename /*filename*/) {
    // Always rewrite the log file
    return 2;
}


#ifndef EKA2
// DLL entry point
GLDEF_C TInt E32Dll(TDllReason)
{
    return(KErrNone);
}
#endif
