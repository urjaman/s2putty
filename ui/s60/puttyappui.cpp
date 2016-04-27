/*    puttyappui.cpp
 *
 * Putty UI Application UI class
 *
 * Copyright 2003-2004 Sergei Khloupnov
 * Copyright 2002-2004 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/


#include <aknquerydialog.h>
#include <aknnotedialog.h>
#include <aknnotewrappers.h>
#include <stringloader.h>
#include <aknmessagequerydialog.h>
#include <aknwaitdialog.h>

#include <eikenv.h>
#include <eikon.hrh>
#include <es_sock.h>
#include <eikmenup.h>
#include <akndoc.h>
#include <aknapp.h>
#include <apgwgnam.h>
#include <apgtask.h>
#include <coeutils.h>

#include "filelistdialog.h"

#include "puttyappui.h"
#include "puttyterminalview.h"
#include "puttyengine.h"
#include "puttyui.hrh"
#include <putty.rsg>
extern "C" {
#include "putty.h"
}

const TInt KInitAudioLength = 32000;
const TInt KExitReason = -1;

_LIT(KAssertPanic, "puttyappui.cpp");
#define assert(x) __ASSERT_ALWAYS(x, User::Panic(KAssertPanic, __LINE__))

_LIT(KRandomFile,"random.dat");
_LIT(KDefaultSettingsFile, "defaults");
_LIT(KFontDir, "fonts");
_LIT(KFontExtension, ".s2f");
_LIT(KDefaultFont, "fixed4x6");

#ifdef __WINS__
_LIT(KPuttyEngine, "puttyengine.dll");
_LIT(KTempParamFile, "c:\\puttystarttemp");
#else
_LIT(KPuttyEngine, "puttyengine.exe");
#endif

#ifdef EKA2
// FIXME: Use data caging directories!
_LIT(KDataDir, "\\system\\apps\\putty\\");
#endif

//_LIT8(KFontNameLabel, "%s %d");

static const TInt KFullScreen = 0xf5;

//#include <e32svr.h>
//#define DEBUGPRINT(x) RDebug::Print x
#define DEBUGPRINT(x)


CPuttyAppUi::CPuttyAppUi()
#ifndef PUTTY_NO_AUDIORECORDER
    : iAudioRecordDes(NULL, 0, 0)
#endif
{
    iTermWidth = 80;
    iTermHeight = 24;
}


void CPuttyAppUi::ConstructL() {

    CEikonEnv::Static()->DisableExitChecks(ETrue);
    BaseConstructL();

    iFatalErrorPanic =
        CEikonEnv::Static()->AllocReadResourceL(R_STR_FATAL_ERROR);
    iConnectedMsg =
        CEikonEnv::Static()->AllocReadResourceL(R_STR_CONNECTING_TO_HOST);

    iDialer = CDialer::NewL(this);
#ifndef PUTTY_NO_AUDIORECORDER
    iRecorder = CAudioRecorder::NewL(this);
#endif

    // Determine application installation path
    HBufC *appDll = HBufC::NewLC(KMaxFileName);
    *appDll = iDocument->Application()->AppFullName();
    TParse parsa;
    User::LeaveIfError(parsa.SetNoWild(*appDll, NULL, NULL));
#ifdef EKA2
    iDataPath = parsa.Drive();
    iDataPath.Append(KDataDir);
#else
    iDataPath = parsa.DriveAndPath();
#endif
    CleanupStack::PopAndDestroy(); // appDll

    // If the application is in ROM (drive Z:), put settings on C:
    if ( (iDataPath[0] == 'Z') || (iDataPath[0] == 'z') ) {
        iDataPath[0] = 'C';
    }

    // Make sure the path ends with a backslash
    if ( iDataPath[iDataPath.Length()-1] != '\\' ) {
        iDataPath.Append('\\');
    }
    DEBUGPRINT((_L("ProcessCommandParametersL: iDataPath = %S"), &iDataPath));

    iFontPath = iDataPath;
    iFontPath.Append(KFontDir);
    iFontPath.Append('\\');

    // Check if the random seed already exists. If not, we are probably running
    // PuTTY for the first time and the generator should be seeded.
    // We'll need to check this before initializing the engine, since it will
    // create the file at startup.
    HBufC *seedFileBuf = HBufC::NewLC(KMaxFileName);
    TPtr seedFile = seedFileBuf->Des();
    seedFile = iDataPath;
    seedFile.Append(KRandomFile);
    iRandomExists = ConeUtils::FileExists(seedFile);
    CleanupStack::PopAndDestroy(seedFileBuf);
    
    // Create and initialize the engine
    iEngine = CPuttyEngine::NewL(this, iDataPath);
    iEngine->SetTerminalSize(iTermWidth, iTermHeight);

    // Check if the default settings file exists. If yes, read it, otherwise
    // create one
    HBufC *settingsFileBuf = HBufC::NewLC(KMaxFileName);
    TPtr settingsFile = settingsFileBuf->Des();
    settingsFile = iDataPath;
    settingsFile.Append(KDefaultSettingsFile);
    if ( ConeUtils::FileExists(settingsFile) ) {
        iEngine->ReadConfigFileL(settingsFile);
        ReadUiSettingsL(iEngine->GetConfig());
    } else {
        iEngine->SetDefaults();
        iFontName = KDefaultFont;
        WriteUiSettingsL(iEngine->GetConfig());
        iEngine->WriteConfigFileL(settingsFile);
    }
    CleanupStack::PopAndDestroy(); //settingsFileBuf;

    // Build a list of possible fonts
    CDir *dir;
    User::LeaveIfError(
        CEikonEnv::Static()->FsSession().GetDir(
            iFontPath, KEntryAttNormal, ESortByName, dir));
    CleanupStack::PushL(dir);
    
    iNumFonts = dir->Count();
    iFonts = new (ELeave) HBufC*[iNumFonts];
    Mem::FillZ((TAny*)iFonts, iNumFonts*sizeof(HBufC*));
    
    for ( TInt i = 0; i < iNumFonts; i++ ) {
        parsa.SetNoWild((*dir)[i].iName, NULL, NULL);
        iFonts[i] = HBufC::NewL(parsa.Name().Length());
        *iFonts[i] = parsa.Name();
    }
    CleanupStack::PopAndDestroy(); //dir
    

    // Build the terminal view
    HBufC *fontFileBuf = HBufC::NewLC(KMaxFileName);
    TPtr fontFile = fontFileBuf->Des();
    fontFile = iFontPath;
    fontFile.Append(iFontName);
    fontFile.Append(KFontExtension);
    iTerminalView = CPuttyTerminalView::NewL(this, this, fontFile);
    AddViewL(iTerminalView); // takes ownership
    SetDefaultViewL(*iTerminalView);
    CleanupStack::PopAndDestroy(); //fontFileBuf;

    // Engine initialization will be completed once the terminal has
    // been created. See TerminalCreatedL().
}


CPuttyAppUi::~CPuttyAppUi() {

    if ( iFonts ) {
        for ( TInt i = 0; i < iNumFonts; i++ ) {
            delete iFonts[i];
        }
        delete iFonts;
    }
    
#ifndef PUTTY_NO_AUDIORECORDER
    delete iRecorder;
    delete iAudio;
#endif
    delete iDialer;
    delete iEngine;
    delete iConnectedMsg;
    delete iFatalErrorPanic;
}

CPuttyEngine* CPuttyAppUi::Engine() {
    return iEngine;
}


// Displays information note
void ShowInformationNoteL(TInt aResourceId) {
    CEikonEnv *eenv = CEikonEnv::Static();
    CAknInformationNote* dlg = new (ELeave) CAknInformationNote();
    dlg->ExecuteLD(eenv->AllocReadResourceLC(aResourceId)->Des());
    CleanupStack::PopAndDestroy(); // allocated string
}


TBool CPuttyAppUi::ProcessCommandParametersL(TApaCommand aCommand, TFileName & aDocumentName, const TDesC8 &aTail) {

    // If we got a config file as a parameter, read it
    if ( aCommand == EApaCommandOpen ) {
        DEBUGPRINT((_L("ProcessCommandParametersL: Reading config file")));
        iEngine->ReadConfigFileL(aDocumentName);
    }

    // Final hack, keep the system happy. This is necessary since we aren't
    // really a proper file-based application.
    DEBUGPRINT((_L("ProcessCommandParametersL: Init done")));
    return CEikAppUi::ProcessCommandParametersL(aCommand, aDocumentName, aTail);
}

TKeyResponse CPuttyAppUi::HandleKeyEventL(const TKeyEvent& /*aKeyEvent*/,
                                          TEventCode /*aType*/ ) {

    return EKeyWasNotConsumed;
}


void CPuttyAppUi::TerminalCreatedL() {
    // Terminal has been created. Finish up initialization
    iTerminal = iTerminalView->Terminal();
    iTerminalView->SetTerminalGrayed(ETrue);

    iLastCommand = EPuttyCmdConnectionConnect;
    
    // Initialize the random number generator if necessary
#ifndef PUTTY_NO_AUDIORECORDER
    if ( !iRandomExists ) {        
        DEBUGPRINT((_L("ProcessCommandParametersL: Initializing random number generator")));
        if ( CAknQueryDialog::NewL()->ExecuteLD(
                 R_INITIAL_RNG_INIT_CONFIRMATION) ) {
            HandleCommandL(EPuttyCmdInitRandomGenerator);
        }
    }
#endif
    // This Fixes FullScreen Status Restore Bug:
    iTerminalView->SetFullScreenL(iFullScreen);
}


void CPuttyAppUi::TerminalDeleted() {
    // Terminal has been deleted
    iTerminal = NULL;
}


void CPuttyAppUi::HandleCommandL(TInt aCommand) {

    TInt saveLastCommand = iLastCommand;

    // Repeat only commands with ID below EPuttyCmdNotRepeated
    if( aCommand < EPuttyCmdNotRepeated ) {
        iLastCommand = aCommand;
    }
    
    Config * config = iEngine->GetConfig();

    // assume there are not more than 100 fonts available
    // this is to not confuse font select cmd with Exit or other
    // pre-defined system commands
    if ( (aCommand >= EPuttyCmdSetFont) &&
         (aCommand < (EPuttyCmdSetFont + 100)) ) {
        assert((aCommand-EPuttyCmdSetFont) < iNumFonts);
        iFontName = *iFonts[aCommand-EPuttyCmdSetFont];
        TFileName fontFile;
        fontFile = iFontPath;
        fontFile.Append(iFontName);
        fontFile.Append(KFontExtension);
        iTerminalView->SetFontL(fontFile);
        return;
    }

    HBufC* text; // will use with StringLoader
    HBufC *buf = HBufC::NewLC(256);
    TPtr ptr = buf->Des();

    switch (aCommand) {

        case EPuttyCmdRepeatLast: {
            iLastCommand = saveLastCommand;
            if ( iLastCommand < EPuttyCmdNotRepeated ) {
                HandleCommandL(iLastCommand);
            }
            break;
        }
            
        case EPuttyCmdConnectionConnect: {
            if ( iState != EPuttyUIStateNone ) {
                ShowInformationNoteL(R_STR_CONNECTION_IN_PROGRESS);
                break;
            }
            assert(iEngine);
            StringToDes(config->host, ptr);
            
            text = StringLoader::LoadLC(R_STRING_HOST);
            CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(ptr);
            dlg->SetPromptL(*text);
            dlg->PrepareLC(R_SETTINGS_STRING_ENTRY);
            dlg->SetMaxLength(128);
            if ( dlg->RunLD() ) {
                DesToString(ptr, config->host, sizeof(config->host));
                iState = EPuttyUIStateDialing;
                ShowDialWaitDialogL();
                iDialer->DialL();
            }
            CleanupStack::PopAndDestroy(); // text
            break;
        }

        case EPuttyCmdConnectionClose: {
            iDialer->CancelDialL();
            break;
        }

        case EPuttyCmdConnectionDisconnect: {
            iEngine->Disconnect();
            assert(iTerminal);
            iTerminalView->SetTerminalGrayed(ETrue);
            iState = EPuttyUIStateDisconnected;
            iDialer->CloseConnectionL();
            break;
        }

        case EPuttyCmdReverseScreen: {
            assert(iTerminal);
             // FIXME: saved in try_palette field
            if( config->try_palette ) {
//                iTerminal->SetReverse(EFalse);
                config->try_palette = 0;
            } else {
//                iTerminal->SetReverse(ETrue);
                config->try_palette = 1;
            }
            break;
        }

        case EPuttyCmdFullScreen: {
            if ( iFullScreen ) {
                iFullScreen = EFalse;
            } else {
                iFullScreen = ETrue;
            }
            iTerminalView->SetFullScreenL(iFullScreen);
            break;
        }

        case EPuttyCmdInitRandomGenerator: {
#ifndef PUTTY_NO_AUDIORECORDER
            if ( iRecording ) {
                ShowInformationNoteL(R_STR_RECORDING_IN_PROGRESS);
                break;
            }
            
            if( CAknQueryDialog::NewL()->ExecuteLD(R_RECORD_CONFIRMATION) ) {
                delete iAudio;
                iAudio = NULL;
                iAudio = HBufC8::NewL(KInitAudioLength);
                iAudioRecordDes.Set(iAudio->Des());
                iRecorder->RecordL(iAudioRecordDes);
                iRecording = ETrue;
                ShowRecordWaitDialogL();
            }
#endif            
            break;
        }

        case EPuttyCmdSettingsConnectionHostPort: {
            assert(iState == EPuttyUIStateNone);
            StringToDes(config->host, ptr);
            CAknMultiLineDataQueryDialog* dlg =
                CAknMultiLineDataQueryDialog::NewL(ptr, config->port);
            if( dlg->ExecuteLD(R_SETTINGS_HOST_PORT_ENTRY) ) {
                DesToString(ptr, config->host, sizeof(config->host));
            }
            break;
        }

        case EPuttyCmdSettingsConnectionVersion: {
            assert(iState == EPuttyUIStateNone);
            assert((config->sshprot >= 0) && (config->sshprot <= 3));
            TInt index(config->sshprot);
            CAknListQueryDialog *dlg =
                new (ELeave) CAknListQueryDialog(&index );
            if ( dlg->ExecuteLD( R_SETTINGS_PROTOCOL ) ) {
                config->sshprot = index;
            }
            break;
        }

        case EPuttyCmdSettingsAuthenticationUsername: {
            assert(iState == EPuttyUIStateNone);
            StringToDes(config->username, ptr);
            CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(ptr);
            text = StringLoader::LoadLC(R_STRING_USERNAME);
            dlg->SetPromptL(text->Des());
            if( dlg->ExecuteLD(R_SETTINGS_STRING_ENTRY) ) {
                DesToString(ptr, config->username, sizeof(config->username));
            }
            CleanupStack::PopAndDestroy(); // text
            break;
        }

        case EPuttyCmdSettingsAuthenticationKeyfile: {
            assert(iState == EPuttyUIStateNone);
            TFileName file;
            StringToDes(config->keyfile.path, file);
            if ( CFileListDialog::RunDlgLD(file, EFalse) ) {
                DesToString(file, config->keyfile.path,
                            sizeof(config->keyfile.path));
            }
            break;
        }

        case EPuttyCmdSettingsLoggingType: {
            assert(iState == EPuttyUIStateNone);
            assert((config->logtype >= 0) && (config->logtype <= 3));
            TInt index(config->logtype);
            CAknListQueryDialog* dlg =
                new (ELeave) CAknListQueryDialog(&index);
            if ( dlg->ExecuteLD( R_SETTINGS_LOG_TYPE ) ) {
                config->logtype = index;
            }
            break;
        }

        case EPuttyCmdSettingsLoggingFile: {
            assert(iState == EPuttyUIStateNone);
            TFileName file;
            StringToDes(config->logfilename.path, file);
            if ( CFileListDialog::RunDlgLD(file, ETrue) ) {
                DesToString(file, config->logfilename.path,
                            sizeof(config->logfilename.path));
            }
            break;
        }

        case EPuttyCmdLoadSettings: {
             assert(iState == EPuttyUIStateNone);
             TFileName file;
             if ( CFileListDialog::RunDlgLD(file, EFalse) ) {
                 iEngine->ReadConfigFileL(file);
                 ReadUiSettingsL(iEngine->GetConfig());
             }
             break;
         }
 
        case EPuttyCmdSaveSettings: {
            assert(iState == EPuttyUIStateNone);
            TFileName file;
            if ( CFileListDialog::RunDlgLD(file, ETrue) ) {
                WriteUiSettingsL(iEngine->GetConfig());
                iEngine->WriteConfigFileL(file);
            }
            break;
        }

        case EPuttyCmdSendLine: {
            if ( iState != EPuttyUIStateConnected )
                break;
            CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(ptr);
            dlg->SetPredictiveTextInputPermitted(ETrue);
            text = StringLoader::LoadLC(R_STRING_LINE);
            dlg->SetPromptL(text->Des());
			
            if( dlg->ExecuteLD(R_SEND_TEXT_DLG) ) {
                int i = 0;
                int len = ptr.Length();
                while ( i < len ) {
                    iEngine->SendKeypress((TKeyCode)ptr[i++], 0);
                }
                iEngine->SendKeypress(EKeyEnter, 0);
            }
            CleanupStack::PopAndDestroy(); // text
            break;
        }

        case EPuttyCmdSendText: {
            if ( iState != EPuttyUIStateConnected )
                break;
            CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(ptr);
            dlg->SetPredictiveTextInputPermitted(ETrue);
            text = StringLoader::LoadLC(R_STRING_TEXT);
            dlg->SetPromptL(text->Des());
			
            if( dlg->ExecuteLD(R_SEND_TEXT_DLG) ) {
                int i = 0;
                int len = ptr.Length();
                while ( i < len ) {
                    iEngine->SendKeypress((TKeyCode)ptr[i++], 0);
                }
            }
            CleanupStack::PopAndDestroy(); // text
            break;
        }

        case EPuttyCmdSendCtrlKeys: {
            if ( iState != EPuttyUIStateConnected )
                break;
            CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(ptr);
            dlg->SetPredictiveTextInputPermitted(EFalse);
            text = StringLoader::LoadLC(R_STRING_CTRL);
            dlg->SetPromptL(text->Des());
			
            if( dlg->ExecuteLD(R_SEND_TEXT_DLG) ) {
                int i = 0;
                int len = ptr.Length();
                while ( i < len ) {
                    iEngine->SendKeypress((TKeyCode)ptr[i++], EModifierCtrl);
                }
            }
            CleanupStack::PopAndDestroy(); // text
            break;
        }

        case EPuttyCmdSendPipe:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)'|', 0);
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;
                
        case EPuttyCmdSendBackquote:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)'`', 0);
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendCR:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress(EKeyEnter, 0);  // enter
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendSpace:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)' ', 0);
            }
            break;

        case EPuttyCmdSendEsc:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)0x1b,
                                      EModifierPureKeycode); // escape
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendTab:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)0x09,
                                      EModifierPureKeycode); // tab
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendCtrlC:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)0x03,
                                      EModifierPureKeycode); // ctrl-c
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendCtrlD:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)0x04,
                                      EModifierPureKeycode); // ctrl-d
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendCtrlZ:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)26,
                                      EModifierPureKeycode); // ctrl-z
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendCtrlAD:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)0x01,
                                      EModifierPureKeycode); // ctrl-a-d
                iEngine->SendKeypress((TKeyCode)'d', 0);
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendCtrlBrkt:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress((TKeyCode)0x1d,
                                      EModifierPureKeycode); // ctrl-]
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendHome:
        case EPuttyCmdSendInsert:
        case EPuttyCmdSendDelete:
        case EPuttyCmdSendEnd:
        case EPuttyCmdSendPageUp:
        case EPuttyCmdSendPageDown:
            if ( iState == EPuttyUIStateConnected ) {
                // FIXME: Move logic to engine
                iEngine->SendKeypress((TKeyCode)0x1b, EModifierPureKeycode);
                iEngine->SendKeypress((TKeyCode)0x5b, EModifierPureKeycode);
                iEngine->SendKeypress(
                    (TKeyCode)(0x31 + aCommand - EPuttyCmdSendHome),
                    EModifierPureKeycode);
                iEngine->SendKeypress((TKeyCode)0x7e, EModifierPureKeycode);
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendAlt0:
        case EPuttyCmdSendAlt1:
        case EPuttyCmdSendAlt2:
        case EPuttyCmdSendAlt3:
        case EPuttyCmdSendAlt4:
        case EPuttyCmdSendAlt5:
        case EPuttyCmdSendAlt6:
        case EPuttyCmdSendAlt7:
        case EPuttyCmdSendAlt8:
        case EPuttyCmdSendAlt9:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress(
                    (TKeyCode)(0x30 + aCommand - EPuttyCmdSendAlt0),
                    EModifierAlt);
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSendAltKeys: {
            if ( iState != EPuttyUIStateConnected )
                break;
            CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(ptr);
            dlg->SetPredictiveTextInputPermitted(EFalse);
            text = StringLoader::LoadLC(R_STRING_ALT);
            dlg->SetPromptL(text->Des());
			
            if( dlg->ExecuteLD(R_SEND_TEXT_DLG) ) {
                int i = 0;
                int len = ptr.Length();
                while ( i < len ) {
                    iEngine->SendKeypress((TKeyCode)ptr[i++], EModifierAlt);
                }
            }
            CleanupStack::PopAndDestroy(); // text
            break;
        }

        case EPuttyCmdSendF1:
        case EPuttyCmdSendF2:
        case EPuttyCmdSendF3:
        case EPuttyCmdSendF4:
        case EPuttyCmdSendF5:
        case EPuttyCmdSendF6:
        case EPuttyCmdSendF7:
        case EPuttyCmdSendF8:
        case EPuttyCmdSendF9:
        case EPuttyCmdSendF10:
            if ( iState == EPuttyUIStateConnected ) {
                iEngine->SendKeypress(
                    (TKeyCode) (EKeyF1 + (aCommand - EPuttyCmdSendF1)), 0);
            } else {
                ShowInformationNoteL(R_STR_NOT_CONNECTED);
            }
            break;

        case EPuttyCmdSaveSettingsAsDefault: {
            TFileName fileName;
            fileName = iDataPath;
            fileName.Append(KDefaultSettingsFile);
            WriteUiSettingsL(iEngine->GetConfig());
            iEngine->WriteConfigFileL(fileName);
            break;
        }

        case EPuttyCmdResetDefaultSettings: {
            iEngine->SetDefaults();
            ReadUiSettingsL(iEngine->GetConfig());
            HandleCommandL(EPuttyCmdSaveSettingsAsDefault);
            break;
        }

        case EPuttyCmdNotImplemented: {
            CAknInformationNote* note = new (ELeave) CAknInformationNote;
            HBufC* text = StringLoader::LoadLC(R_NOT_IMPLEMENTED);
            note->ExecuteLD(text->Des());
            CleanupStack::PopAndDestroy(text);
            break;
        }

        case EAknSoftkeyBack:
        case EAknSoftkeyExit:
        case EEikCmdExit: {
            if ( iState == EPuttyUIStateConnected ) {
                if(CAknQueryDialog::NewL()->ExecuteLD(R_REALLY_EXIT)) {
                    CleanupStack::PopAndDestroy(); // buf
                    Exit();
                }
            } else {
                CleanupStack::PopAndDestroy(); // buf
                Exit();
            }

            break;
        }

        default:
            break;
    }    
    CleanupStack::PopAndDestroy(); // buf
}


void CPuttyAppUi::DoDynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane) {

#define DIMNOTIDLE(x) aMenuPane->SetItemDimmed((x), (iState != EPuttyUIStateNone))

    switch ( aResourceId ) {
        case R_PUTTY_MENU_PANE:
            aMenuPane->SetItemDimmed(EPuttyCmdSend,
                                     (iState != EPuttyUIStateConnected));
            break;
            
        case R_PUTTY_SETTINGS_MENU:
            DIMNOTIDLE(EPuttyCmdSettingsConnection);
            DIMNOTIDLE(EPuttyCmdSettingsAuthentication);
            DIMNOTIDLE(EPuttyCmdSettingsLogging);
            DIMNOTIDLE(EPuttyCmdLoadSettings);
            DIMNOTIDLE(EPuttyCmdSaveSettings);
            DIMNOTIDLE(EPuttyCmdSaveSettingsAsDefault);
            DIMNOTIDLE(EPuttyCmdResetDefaultSettings);
            break;

        case R_PUTTY_CONNECTION_MENU:
            DIMNOTIDLE(EPuttyCmdConnectionConnect);
            aMenuPane->SetItemDimmed(EPuttyCmdConnectionClose,
                                     ((iState == EPuttyUIStateNone) ||
                                      (iState == EPuttyUIStateDisconnected)));
            aMenuPane->SetItemDimmed(EPuttyCmdConnectionDisconnect,
                                     (iState != EPuttyUIStateConnected));
            break;

        case R_PUTTY_SETTINGS_FONT: {
            // Add fonts to the menu, with commands starting from
            // EPuttyCmdSetFont

            for ( TInt i = 0; i < iNumFonts; i++ ) {
                CEikMenuPaneItem::SData item;
                item.iText = *iFonts[i];
                item.iCommandId = EPuttyCmdSetFont + i;
                item.iFlags = 0;
                item.iCascadeId = 0;
                aMenuPane->AddMenuItemL(item);                
            }
            break;
        }

        default:
            ; // Do nothing, keep compiler happy
    }
}


// Reads the UI settings (font size, full screen flag) from a config structure
void CPuttyAppUi::ReadUiSettingsL(Config *aConfig) {

    // Get the font to use
    StringToDes(aConfig->font.name, iFontName);

    // Check that the file exists. If not, use the default font
    TFileName fontFile;
    fontFile = iFontPath;
    fontFile.Append(iFontName);
    fontFile.Append(KFontExtension);
    if ( !ConeUtils::FileExists(fontFile) ) {
        iFontName = KDefaultFont;
        fontFile = iFontPath;
        fontFile.Append(KDefaultFont);
        fontFile.Append(KFontExtension);
        assert(ConeUtils::FileExists(fontFile));
    }

    
    // Determine if the display should be full screen or not
    if( aConfig->width == KFullScreen ) {
        iFullScreen = ETrue;
    } else {
        iFullScreen = EFalse;
    }

    if ( iTerminalView ) {
//        iTerminalView->SetFontL(fontFile);
        iTerminalView->SetFullScreenL(iFullScreen);
    }

    // FIXME: Reverse disabled for now
    /*
    if( aConfig->try_palette ) {
        iTerminal->SetReverse(ETrue);
    } else {
        iTerminal->SetReverse(EFalse);
    }
    */
}


// Writes the UI settings (font size, full screen flag) to a config structure
void CPuttyAppUi::WriteUiSettingsL(Config *aConfig) {

    
    TPtr8 fontPtr((TUint8*)aConfig->font.name, sizeof(aConfig->font.name));
    fontPtr.Copy(iFontName);
    fontPtr.Append('\0');

    if( iFullScreen ) {
        aConfig->width = KFullScreen;
    } else {
        aConfig->width = KFullScreen - KFullScreen;
    }
}


// MDialObserver::DialCompleted()
// Called by CDialer when connection status changes
void CPuttyAppUi::DialCompletedL(TInt aError) {

    assert(iState == EPuttyUIStateDialing);
    iState = EPuttyUIStateConnecting;
    
    CEikonEnv *eenv = CEikonEnv::Static();    
    RemoveDialWaitDialogL();
    
    if ( aError != KErrNone ) {
        HBufC *msg = HBufC::NewLC(256);
        TPtr msgp = msg->Des();
        HBufC *err = HBufC::NewLC(128);
        TPtr errp = err->Des();
            
        eenv->GetErrorText(errp, aError);
        msgp.Format(*eenv->AllocReadResourceLC(R_STR_CONNECTION_FAILED),
                    &errp, aError);
            
        CAknNoteDialog* dlg = new( ELeave ) CAknNoteDialog();
        dlg->SetTextL(msgp);
        dlg->ExecuteDlgLD(R_INFO_MESSAGE);

        CleanupStack::PopAndDestroy(3); // formatstring, err, msg            
        iState = EPuttyUIStateNone;
        return;
    }

    // Show a "Connecting to host" note before proceeding. This slows the connection
    // process slightly, but the dialog has to be a modal one to make it
    // visible at all, otherwise it wouldn't get shown before the PuTTY
    // engine connection process starts. Since the engine connection is
    // synchronous and can take quite a while, it's useful to give feedback
    // to the user at this point.
    CAknInformationNote *note = new (ELeave) CAknInformationNote(ETrue);
    note->SetTone(CAknInformationNote::ENoTone);
    note->SetTimeout(CAknInformationNote::EShortTimeout);
    note->ExecuteLD(*iConnectedMsg);
    
    TInt err = iEngine->Connect();
    if ( err != KErrNone ) {
        TBuf<128> msg;
        iEngine->GetErrorMessage(msg);
        ConnectionErrorL(msg);
        // FIXME: We should be in a state where we can exit more cleanly
        User::Exit(KExitReason);
    }
    
    iState = EPuttyUIStateConnected;
    assert(iTerminal);
    iTerminalView->SetTerminalGrayed(EFalse);
}


#ifndef PUTTY_NO_AUDIORECORDER
// MRecordObserver::RecordCompleted()
void CPuttyAppUi::RecordCompleted(TInt aError) {

    RemoveRecordWaitDialogL();
    
    CEikonEnv *eenv = CEikonEnv::Static();

    // If the audio device was reserved, prompt to try again
    if ( aError == KErrInUse ) {

        if ( CAknQueryDialog::NewL()->ExecuteLD(R_RECORDER_IN_USE) ) {
            iRecorder->CancelRecord();
            iAudioRecordDes.SetLength(0);
            iRecorder->RecordL(iAudioRecordDes);
            iRecording = ETrue;
            return;
        }
        
    } else if ( aError != KErrNone ) {
        // Handle other errors                    
        iRecorder->CancelRecord();
        
        HBufC *msg = HBufC::NewLC(512);
        TPtr msgp = msg->Des();
        HBufC *err = HBufC::NewLC(128);
        TPtr errp = err->Des();        
        eenv->GetErrorText(errp, aError);
        msgp.Format(*eenv->AllocReadResourceLC(R_STR_RECORD_FAILED), &errp);
        
        CAknNoteDialog* dlg = new( ELeave ) CAknNoteDialog();
        dlg->SetTextL( msgp );
        dlg->ExecuteDlgLD(R_INFO_MESSAGE);

        CleanupStack::PopAndDestroy(3); // formatstring, err, msg

    } else {
        // Use the data as random number seed noise.
        iEngine->AddRandomNoise(iAudioRecordDes);
        ShowInformationNoteL(R_STR_RANDOMIZED);
    }

    delete iAudio;
    iAudio = NULL;
    iRecording = EFalse;        
}
#endif


// MPuttyClient::DrawText()
void CPuttyAppUi::DrawText(TInt aX, TInt aY, const TDesC &aText, TBool aBold,
                           TBool aUnderline, TRgb aForeground,
                           TRgb aBackground) {

    if ( iTerminal ) {
        iTerminal->DrawText(aX, aY, aText, aBold, aUnderline, aForeground,
                            aBackground);
    }
}


// MPuttyClient::SetCursor()
void CPuttyAppUi::SetCursor(TInt aX, TInt aY) {
    if ( iTerminal ) {
        iTerminal->SetCursor(aX, aY);
    }
}


// MPuttyClient::ConnectionError()
void CPuttyAppUi::ConnectionError(const TDesC &aMessage) {

    TRAPD(error, ConnectionErrorL(aMessage));
    if ( error != KErrNone ) {
        User::Panic(*iFatalErrorPanic, error);
    }
    if ( iTerminal ) {
        iTerminalView->SetTerminalGrayed(ETrue);
    }
}

void CPuttyAppUi::ConnectionErrorL(const TDesC &aMessage) {

    CAknNoteDialog* dlg = new( ELeave ) CAknNoteDialog();
    dlg->SetTextL( aMessage );
    dlg->ExecuteDlgLD(R_INFO_MESSAGE);
    User::Exit(KExitReason);
}


// MPuttyClient::FatalError()
void CPuttyAppUi::FatalError(const TDesC &aMessage) {

    TRAPD(error, FatalErrorL(aMessage));
    if ( error != KErrNone ) {
        User::Panic(*iFatalErrorPanic, error);
    }
}

void CPuttyAppUi::FatalErrorL(const TDesC &aMessage) {

    CAknNoteDialog* dlg = new( ELeave ) CAknNoteDialog();
    dlg->SetTextL( aMessage );
    dlg->ExecuteDlgLD(R_INFO_MESSAGE);
    User::Exit(KExitReason);
}


// MPuttyClient::ConnectionClosed()
void CPuttyAppUi::ConnectionClosed() {
    ShowInformationNoteL(R_STR_CONNECTION_CLOSED);
    if ( iTerminal ) {
        iTerminalView->SetTerminalGrayed(ETrue);
    }
    iState = EPuttyUIStateDisconnected;
}


// MPuttyClient::UnknownHostKey()
MPuttyClient::THostKeyResponse CPuttyAppUi::UnknownHostKey(
    const TDesC &aFingerprint) {
    
    MPuttyClient::THostKeyResponse resp = EAbadonConnection;
    TRAPD(error, resp = HostKeyDialogL(aFingerprint,
                                       R_STR_UNKNOWN_HOST_KEY_DLG_FMT));
    if ( error != KErrNone ) {
        User::Panic(*iFatalErrorPanic, error);
    }
    return resp;
}


// MPuttyClient::DifferentHostKey()
MPuttyClient::THostKeyResponse CPuttyAppUi::DifferentHostKey(
    const TDesC &aFingerprint) {

    MPuttyClient::THostKeyResponse resp = EAbadonConnection;
    TRAPD(error, resp = HostKeyDialogL(aFingerprint,
                                       R_STR_DIFFERENT_HOST_KEY_DLG_FMT));
    if ( error != KErrNone ) {
        User::Panic(*iFatalErrorPanic, error);
    }
    return resp;
}


MPuttyClient::THostKeyResponse CPuttyAppUi::HostKeyDialogL(
    const TDesC &aFingerprint, TInt aDialogFormatRes) {

    CEikonEnv *env = CEikonEnv::Static();

    HBufC *fmt = env->AllocReadResourceLC(aDialogFormatRes);
    HBufC *contents = HBufC::NewLC(fmt->Length() + aFingerprint.Length());
    contents->Des().Format(*fmt, &aFingerprint);

    MPuttyClient::THostKeyResponse resp = EAbadonConnection;

    CAknQueryDialog *dlg = CAknQueryDialog::NewL();
    dlg->SetPromptL(*contents);
    if ( dlg->ExecuteLD(R_HOSTKEY_QUERY) ) {
        resp = EAcceptAndStore;
    } else {
        resp = EAbadonConnection;
    }
    
    CleanupStack::PopAndDestroy(2); // contents, fmt

    return resp;
}


// MPuttyClient::AcceptCipher()
TBool CPuttyAppUi::AcceptCipher(const TDesC &aCipherName,
                                TCipherDirection aDirection) {
    TBool resp = EFalse;
    TRAPD(error, resp = AcceptCipherL(aCipherName, aDirection));
    if ( error != KErrNone ) {
        User::Panic(*iFatalErrorPanic, error);
    }
    return resp;
}


TBool CPuttyAppUi::AcceptCipherL(const TDesC &aCipherName,
                                 TCipherDirection aDirection) {
    
    CEikonEnv *env = CEikonEnv::Static();

    HBufC *fmt = env->AllocReadResourceLC(R_STR_ACCEPT_CIPHER_DLG_FMT);
    HBufC *dir = NULL;

    switch ( aDirection ) {
        case EBothDirections:
            dir = env->AllocReadResourceLC(R_STR_ACCEPT_CIPHER_DIR_BOTH);
            break;

        case EClientToServer:
            dir = env->AllocReadResourceLC(
                R_STR_ACCEPT_CIPHER_CLIENT_TO_SERVER);
            break;

        case EServerToClient:
            dir = env->AllocReadResourceLC(
                R_STR_ACCEPT_CIPHER_SERVER_TO_CLIENT);
            break;

        default:
            assert(EFalse);
    }

    HBufC *contents = HBufC::NewLC(fmt->Length() +
                                   aCipherName.Length() + dir->Length());
    contents->Des().Format(*fmt, &aCipherName, dir);

    TBool res = CAknQueryDialog::NewL()->ExecuteLD(R_ACCEPT_WEAK_CIPHER);

    CleanupStack::PopAndDestroy(3); // contents, dir, fmt
    return res;
}


TBool CPuttyAppUi::AuthenticationPrompt(const TDesC &aPrompt, TDes &aTarget,
                                        TBool aSecret) {
    TBool ret = EFalse;
    TRAPD(error, ret = AuthenticationPromptL(aPrompt, aTarget, aSecret));
    if ( error != KErrNone ) {
        User::Panic(*iFatalErrorPanic, error);
    }
    return ret;
}

    
TBool CPuttyAppUi::AuthenticationPromptL(const TDesC &aPrompt, TDes &aTarget,
                                        TBool aSecret) {

    assert(iState == EPuttyUIStateConnected);

    CAknTextQueryDialog *dlg = CAknTextQueryDialog::NewL(aTarget);
    
    CleanupStack::PushL(dlg);
    dlg->SetPromptL(aPrompt);
    CleanupStack::Pop();
    
    if ( aSecret ) {
        return dlg->ExecuteLD(R_AUTH_DLG_SECRET);
    }
    return dlg->ExecuteLD(R_AUTH_DLG_NOT_SECRET);
}


// MTerminalObserver::TerminalSizeChanged()
void CPuttyAppUi::TerminalSizeChanged(TInt aWidth, TInt aHeight) {
    assert((aWidth > 1) && (aHeight > 1));
    iTermWidth = aWidth;
    iTermHeight = aHeight;
    if ( iEngine ) {
        iEngine->SetTerminalSize(aWidth, aHeight);
    }
}


// MTerminalObserver::KeyPressed()
void CPuttyAppUi::KeyPressed(TKeyCode aCode, TUint aModifiers) {
    if ( iEngine ) {
        iEngine->SendKeypress(aCode, aModifiers);
    }
}


// MTerminalObserver::RePaintWindow()
void CPuttyAppUi::RePaintWindow() {
    if ( iEngine ) {
        iEngine->RePaintWindow();
    }
}


// MProgressDialogCallback::DialogDismissedL()
void CPuttyAppUi::DialogDismissedL(TInt /*aButtonId*/) {

    iDialWaitDialogOpen = EFalse;
    if ( iState == EPuttyUIStateDialing ) {
        iDialer->CancelDialL();
        iState = EPuttyUIStateNone;
    }
}


// Converts a descriptor to a null-terminated C string.
void CPuttyAppUi::DesToString(const TDesC &aDes, char *aTarget,
                              int targetLen) {
    int i = 0;
    int len = aDes.Length();
    assert(len < (targetLen-1));
    while ( i < len ) {
        TChar c = aDes[i];
        if ( c > 0x7f ) {
            c = '?';
        }
        *aTarget++ = (char) c;
        i++;
    }
    *aTarget = 0;
}


// Converts a null-terminated string to a descriptor. Doesn't support anything
// except 7-bit ASCII.
void CPuttyAppUi::StringToDes(const char *aStr, TDes &aTarget) {
    aTarget.SetLength(0);
    while ( *aStr ) {
        TChar c = *aStr++;
        if ( c > 0x7f ) {
            c = '?';
        }
        aTarget.Append(c);
    }
}


// Shows the connection establishment wait dialog
void CPuttyAppUi::ShowDialWaitDialogL() {

    // Note! This backwards way of using and dismissing the wait dialog is
    // apparently due to a bug in some 7650 system software versions.
    // From the S60 SDK Note control example, aknexnotecontrol.cpp:
        // 1st parameter is used by only ASSERT in Sw 4.0.16.
        // If NULL is passed as the perameter, crash does not occur.   
    CAknWaitDialog *dlg = new (ELeave) CAknWaitDialog(NULL, ETrue);
    dlg->SetCallback(this);
    dlg->ExecuteLD(R_DIALER_WAIT_DIALOG);
    iDialWaitDialogOpen = ETrue;
}


// Dismisses the connection establishment wait dialog
void CPuttyAppUi::RemoveDialWaitDialogL() {

    // Check that the dialog is still open
    if ( (!iDialWaitDialogOpen) || (!IsDisplayingMenuOrDialog()) ) {
        iDialWaitDialogOpen = EFalse;
        return;
    }

    // Send a simulated escape, hopefully it gets to the dialog...
    TKeyEvent key;
    key.iCode = EKeyEscape;
    key.iModifiers = 0;
    CEikonEnv::Static()->SimulateKeyEventL(key, EEventKey);

    iDialWaitDialogOpen = EFalse;
}


// Shows the recording wait dialog
void CPuttyAppUi::ShowRecordWaitDialogL() {
    // See comments in ShowDialWaitDialogL() and RemoveDialWaitDialogL()...
    CAknWaitDialog *dlg = new (ELeave) CAknWaitDialog(NULL, ETrue);
    dlg->SetCallback(this);
    dlg->ExecuteLD(R_RECORDING_WAIT_DIALOG);
}


// Dismisses the recording wait dialog
void CPuttyAppUi::RemoveRecordWaitDialogL() {
    if ( IsDisplayingMenuOrDialog() ) {
        TKeyEvent key;
        key.iCode = EKeyEscape;
        key.iModifiers = 0;
        CEikonEnv::Static()->SimulateKeyEventL(key, EEventKey);
    }
}
