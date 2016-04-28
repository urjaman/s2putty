/*    profilelistview.cpp
 *
 * Putty profile list view
 *
 * Copyright 2007,2010 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <aknviewappui.h>
#include <aknlists.h>
#include <badesca.h>
#include <putty.rsg>
#include <akntitle.h>
#include <bautils.h>
#include <f32file.h>
#include <aknnotedialog.h>
#include <akncommondialogs.h>
#include "profilelistview.h"
#ifdef PROFILELISTVIEW_TOOLBAR
#include <akntoolbar.h>
#endif
#ifdef PUTTY_S60TOUCH
#include "touchuisettings.h"
#endif
#include "puttyappui.h"
#include "puttyengine.h"
#include "stringutils.h"
#include "puttyuids.hrh"
#include "puttyui.hrh"
#include "../common/logfile.h"

_LIT(KListBoxFormat, "\t%S\t\t");
const TInt KListBoxFormatAddLength = 3;

_LIT(KDefaultProfileName, "Default");
_LIT(KBlankProfileName, "Profile");
_LIT(KNewProfileName, "New Profile");
_LIT(KUniqueProfileFormat, "%S (%d)");
const TInt KDefaultProfileIndex = 0;

const TInt KFatalErrorExit = -1;

_LIT(KDefaultFont, "fixed6x13");


// Factory
CProfileListView *CProfileListView::NewL() {
    CProfileListView *self = new (ELeave) CProfileListView;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}


// Constructor
CProfileListView::CProfileListView() {
}


// Second-phase constructor
void CProfileListView::ConstructL() {
    BaseConstructL(R_PUTTY_PROFILELIST_VIEW);
#ifdef PUTTY_S60TOUCH
    iSettings = new (ELeave) TTouchSettings;
#endif
}


// Destructor
CProfileListView::~CProfileListView() {
    if ( iListBox ) {
        AppUi()->RemoveFromStack(iListBox);
    }
    delete iListBox;
    iListBox = NULL;
    delete iProfileFileArray;
    iProfileFileArray = NULL;
    delete iProfileListArray;
    iProfileListArray = NULL;
    delete iPutty;
    iPutty = NULL;
#ifdef PUTTY_S60TOUCH
    delete iSettings;
#endif
}


// CAknView::Id()
TUid CProfileListView::Id() const {
    return TUid::Uid(KUidPuttyProfileListViewDefine);
}


// CAknView::HandleCommandL
void CProfileListView::HandleCommandL(TInt aCommand) {
    
    switch ( aCommand ) {

        case EPuttyCmdProfileListConnect:
            if ( iListBox && (iProfileFileArray->Count() > 0) ) {
                // Delete our PuTTY engine instance, since currently multiple
                // simultaneous instances won't work
                delete iPutty;
                iPutty = NULL;
                iSelectedItem = iListBox->CurrentItemIndex();
                HBufC *proffile = AbsoluteFileNameLC(
                    (*iProfileFileArray)[iSelectedItem]);
                HBufC *setfile = SettingsFileNameLC(
                    (*iProfileFileArray)[iSelectedItem]);
#ifdef PUTTY_S60TOUCH
                if ( !BaflUtils::FileExists(CEikonEnv::Static()->FsSession(),
                                            *setfile) ) {
                    // If there is settings file corresponding to this profile,
                    // create one with defaults. This happens when migrating
                    // from old-style global UI settings file to
                    // profile-specific ones
                    iSettings->SetDefault();
                    iSettings->WriteSettingFileL(*setfile);
                }
#endif
                ((CPuttyAppUi*)AppUi())->ActivateTerminalViewL(*proffile,
                                                               *setfile);
                CleanupStack::PopAndDestroy(2);
            }
            break;

        case EPuttyCmdProfileListEdit:
            if ( iListBox && (iProfileFileArray->Count() > 0) ) {
                iSelectedItem = iListBox->CurrentItemIndex();
                iProfileEditName = (*iProfileFileArray)[iSelectedItem];
                iProfileOldName = iProfileEditName;
                iPutty->ReadConfigFileL(*AbsoluteFileNameLC(iProfileEditName));
                CleanupStack::PopAndDestroy();
#ifdef PUTTY_S60TOUCH
                HBufC *sfile = SettingsFileNameLC(iProfileEditName);
                if ( BaflUtils::FileExists(CEikonEnv::Static()->FsSession(),
                                           *sfile) ) {
                    iSettings->ReadSettingFileL(*sfile);
                } else {
                    // If there is settings file corresponding to this profile,
                    // create one with defaults. This happens when migrating
                    // from old-style global UI settings file to
                    // profile-specific ones
                    iSettings->SetDefault();
                    iSettings->WriteSettingFileL(*sfile);
                }
                CleanupStack::PopAndDestroy();
#endif
                ((CPuttyAppUi*)AppUi())->ActivateProfileEditViewL(
                    *iPutty, iProfileEditName, *iSettings);
            }
            break;
        
        case EPuttyCmdProfileListNew:
            if ( iListBox ) {
                // New profile -- start with defaults, but with a new name
                // Note that the engine does not set the font name since it's
                // UI-specific
                iPutty->SetDefaults();
                Config *cfg = iPutty->GetConfig();
                DesToString(KDefaultFont, cfg->font.name,
                            sizeof(cfg->font.name));
                iProfileEditName = KNewProfileName;
                MakeNameUniqueL(iProfileEditName);
                iPutty->WriteConfigFileL(
                    *AbsoluteFileNameLC(iProfileEditName));
                CleanupStack::PopAndDestroy();
#ifdef PUTTY_S60TOUCH
                iSettings->SetDefault();
                iSettings->WriteSettingFileL(
                    *SettingsFileNameLC(iProfileEditName));
                CleanupStack::PopAndDestroy();
#endif    
                AppendProfileL(iProfileEditName);
                iListBox->HandleItemAdditionL();
                iSelectedItem = iProfileFileArray->Count()-1;
                iListBox->SetCurrentItemIndex(iSelectedItem);
                iListBox->ScrollToMakeItemVisible(iSelectedItem);
                iListBox->DrawDeferred();

                // Edit the new profile
                iProfileOldName = iProfileEditName;
                ((CPuttyAppUi*)AppUi())->ActivateProfileEditViewL(
                    *iPutty, iProfileEditName, *iSettings);
            }
            break;
        
        case EPuttyCmdProfileListDelete:
            if ( iListBox && (iProfileFileArray->Count() > 0) ) {
                // Delete profile file
                TInt sel = iListBox->CurrentItemIndex();
                User::LeaveIfError(
                    CEikonEnv::Static()->FsSession().Delete(
                        *AbsoluteFileNameLC((*iProfileFileArray)[sel])));
                CleanupStack::PopAndDestroy();
#ifdef PUTTY_S60TOUCH
                User::LeaveIfError(
                    CEikonEnv::Static()->FsSession().Delete(
                        *SettingsFileNameLC((*iProfileFileArray)[sel])));
                CleanupStack::PopAndDestroy();
#endif                
                
                // Remove the profile from the list and update listbox
                iProfileFileArray->Delete(sel);
                iProfileListArray->Delete(sel);
                iListBox->HandleItemRemovalL();
                if ( sel >= iProfileFileArray->Count() ) {
                    sel = iProfileFileArray->Count() - 1;
                }
                if ( sel < 0 ) {
                    sel = 0;
                }
                iListBox->SetCurrentItemIndex(sel);
                iListBox->DrawDeferred();
            }
            break;

        case EPuttyCmdProfileListExport:
            if ( iListBox && (iProfileFileArray->Count() > 0) ) {
                // Run "Save As" dialog to get file name
                TInt sel = iListBox->CurrentItemIndex();
                TFileName name = (*iProfileFileArray)[sel];
                if ( AknCommonDialogs::RunSaveDlgLD(
                         name, R_PUTTY_MEMORY_SELECTION_DIALOG,
                         *(CCoeEnv::Static()->AllocReadResourceLC(
                               R_PUTTY_STR_EXPORT_DIALOG_TITLE)),
                         *(CCoeEnv::Static()->AllocReadResourceLC(
                               R_PUTTY_STR_EXPORT_PROMPT))) ) {

                    // Copy the profile to the selected file, overwriting if
                    // necessary
                    CFileMan *fman = CFileMan::NewL(
                        CEikonEnv::Static()->FsSession());
                    CleanupStack::PushL(fman);
                    User::LeaveIfError(
                        fman->Copy(
                            *AbsoluteFileNameLC((*iProfileFileArray)[sel]),
                            name));
                    CleanupStack::PopAndDestroy(2); // name, fman
                }
                CleanupStack::PopAndDestroy(2); //title, prompt
            }
            break;

        case EPuttyCmdProfileListImport:
            if ( iListBox ) {
                // Show file selection dialog for file
                TFileName name;
                if ( AknCommonDialogs::RunSelectDlgLD(
                         name, R_PUTTY_MEMORY_SELECTION_DIALOG) ) {

                    // Use the file name as the profile name
                    TParsePtr parse(name);
                    iProfileEditName = parse.Name();
                    MakeNameUniqueL(iProfileEditName);

                    // Copy the profile to the new file
                    CFileMan *fman = CFileMan::NewL(
                        CEikonEnv::Static()->FsSession());
                    CleanupStack::PushL(fman);
                    User::LeaveIfError(
                        fman->Copy(name, 
                                   *AbsoluteFileNameLC(iProfileEditName)));
                    CleanupStack::PopAndDestroy(2); // name, fman
                    
#ifdef PUTTY_S60TOUCH
                    // Create blank UI settings for the imported profile
                    iSettings->SetDefault();
                    iSettings->WriteSettingFileL(
                        *SettingsFileNameLC(iProfileEditName));
                    CleanupStack::PopAndDestroy();
#endif

                    // Add to the list
                    AppendProfileL(iProfileEditName);
                    iListBox->HandleItemAdditionL();
                    iSelectedItem = iProfileFileArray->Count()-1;
                    iListBox->SetCurrentItemIndex(iSelectedItem);
                    iListBox->ScrollToMakeItemVisible(iSelectedItem);
                    iListBox->DrawDeferred();
                };
            };
            break;
        
        default:
            // Forward unknown commands to the app ui
            AppUi()->HandleCommandL(aCommand);
    }
}


// CAknView::DoActivateL
void CProfileListView::DoActivateL(const TVwsViewId &aPrevViewId,
                                   TUid /*aCustomMessageId*/,
                                   const TDesC8 & /*aCustomMessage*/) {

    if ( !iProfileListArray ) {
        // Arrays not created yet -- starting for the first time.
        InitViewL();
        
    } else {
        // Starting again. If we're coming from the profile edit view, handle
        // possible changes.
        if ( aPrevViewId.iViewUid ==
             TUid::Uid(KUidPuttyProfileEditViewDefine) ) {
            
            // Handle possible renames first. We'll rewrite the settings in any
            // case, so we just ensure the name is right and get rid of the old
            // file.
            if ( iProfileOldName.Compare(iProfileEditName) != 0 ) {
                // Delete current file so that we can rename to a file
                // that maps to the same name (e.g. changes in case)
                User::LeaveIfError(CEikonEnv::Static()->FsSession().Delete(
                                       *AbsoluteFileNameLC(iProfileOldName)));
                CleanupStack::PopAndDestroy();
#ifdef PUTTY_S60TOUCH
                User::LeaveIfError(CEikonEnv::Static()->FsSession().Delete(
                                       *SettingsFileNameLC(iProfileOldName)));
                CleanupStack::PopAndDestroy();
#endif

                // Fix the name if necessary
                MakeNameLegal(iProfileEditName);
                MakeNameUniqueL(iProfileEditName);
                
                // Update profile list and listbox
                iProfileFileArray->Delete(iSelectedItem);
                iProfileListArray->Delete(iSelectedItem);
                iListBox->HandleItemRemovalL();
                iProfileFileArray->InsertL(iSelectedItem, iProfileEditName);
                iProfileListArray->InsertL(iSelectedItem,
                                           *FormatForListboxLC(iProfileEditName));
                iListBox->HandleItemAdditionL();
                CleanupStack::PopAndDestroy();
            }

            // Save the settings, using updated name if necessary
            iPutty->WriteConfigFileL(*AbsoluteFileNameLC(iProfileEditName));
            CleanupStack::PopAndDestroy();
#ifdef PUTTY_S60TOUCH
            iSettings->WriteSettingFileL(*SettingsFileNameLC(iProfileEditName));
            CleanupStack::PopAndDestroy();
#endif
            
        } else {
            // Coming from the terminal view, probably better off with a fresh
            // engine instance
            delete iPutty;
            iPutty = NULL;
            iPutty = CPuttyEngine::NewL(this, iDataDirectory);
        }
    }

    // Set title
    HBufC *title = CEikonEnv::Static()->AllocReadResourceL(
        R_PUTTY_STR_PROFILELIST_TITLE);
    CAknTitlePane* titlePane = static_cast<CAknTitlePane*>
        (StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle)));
    titlePane->SetText(title); //takes ownership
}


// CAknView::DoDeactivate()
void CProfileListView::DoDeactivate() {
    
}


// CAknView::HandleStatusPaneSizeChange()
void CProfileListView::HandleStatusPaneSizeChange() {
    if ( iListBox ) {
        iListBox->SetRect(ClientRect());
    }
}


// Format profile name for use in the listbox
HBufC *CProfileListView::FormatForListboxLC(const TDesC &aProfileFileName) {
    HBufC *buf = HBufC::NewLC(aProfileFileName.Length() +
                              KListBoxFormatAddLength);
    TPtr p = buf->Des();
    p.Format(KListBoxFormat, &aProfileFileName);
    return buf;
}


// Build an absolute file name for a profile file
HBufC *CProfileListView::AbsoluteFileNameLC(const TDesC &aProfileFileName) {
    HBufC *buf = HBufC::NewLC(aProfileFileName.Length() +
                              iProfileDirectory.Length());
    TPtr p = buf->Des();
    p = iProfileDirectory;
    p.Append(aProfileFileName);
    return buf;
}


// Build an absolute file name for a UI settings file
HBufC *CProfileListView::SettingsFileNameLC(const TDesC &aProfileFileName) {
    HBufC *buf = HBufC::NewLC(aProfileFileName.Length() +
                              iSettingsDirectory.Length());
    TPtr p = buf->Des();
    p = iSettingsDirectory;
    p.Append(aProfileFileName);
    return buf;
}


// Append a profile to both lists
void CProfileListView::AppendProfileL(const TDesC &aProfileFileName) {
    // Add to file name array
    iProfileFileArray->AppendL(aProfileFileName);
    iProfileListArray->AppendL(*FormatForListboxLC(aProfileFileName));
    CleanupStack::PopAndDestroy();
}


// Initialize for the first time
void CProfileListView::InitViewL() {

#ifdef PROFILELISTVIEW_TOOLBAR
    iToolbar = Toolbar();
    iToolbar->SetToolbarVisibility(ETrue, EFalse);
    iToolbar->SetToolbarObserver(this);
#endif
    LFPRINT((_L("InitViewL")));
    
    iProfileFileArray = new (ELeave) CDesCArrayFlat(8);
    iProfileListArray = new (ELeave) CDesCArrayFlat(8);

    LFPRINT((_L("InitViewL 1")));

    RFs &fs = CEikonEnv::Static()->FsSession();

    // Get directories from the app UI
    iDataDirectory = ((CPuttyAppUi*)AppUi())->DataDirectory();
    iProfileDirectory = ((CPuttyAppUi*)AppUi())->ProfileDirectory();
    iSettingsDirectory =  ((CPuttyAppUi*)AppUi())->SettingsDirectory();

    LFPRINT((_L("InitViewL 2")));

    // Create an engine instance
    iPutty = CPuttyEngine::NewL(this, iDataDirectory);

    LFPRINT((_L("InitViewL 3")));

    // Find all profile files from the profile directory and add them to the
    // list    
    CDir *dir;
    User::LeaveIfError(fs.GetDir(iProfileDirectory, KEntryAttNormal,
                                 ESortByName, dir));
    CleanupStack::PushL(dir);
    for ( TInt i = 0; i < dir->Count(); i++ ) {
        AppendProfileL((*dir)[i].iName);
    }
    CleanupStack::PopAndDestroy(); //dir
    LFPRINT((_L("InitViewL 4")));

    // If there are no profile files at all, create a default
    if ( iProfileFileArray->Count() == 0 ) {
	LFPRINT((_L("Creating a default")));
        iPutty->SetDefaults();
        iPutty->WriteConfigFileL(*AbsoluteFileNameLC(KDefaultProfileName));
        CleanupStack::PopAndDestroy();
        AppendProfileL(KDefaultProfileName);
#ifdef PUTTY_S60TOUCH
        // Create default UI settings file too
        TTouchSettings settings;
        settings.WriteSettingFileL(*SettingsFileNameLC(KDefaultProfileName));
        CleanupStack::PopAndDestroy();
#endif
    }

    // Create the listbox -- this is the only control this view uses, so no
    // separate container needed
    iListBox = new (ELeave) CAknSingleStyleListBox();
    iListBox->ConstructL(NULL); // Creates a window since there is no parent
    iListBox->CreateScrollBarFrameL(ETrue);
    iListBox->ScrollBarFrame()->SetScrollBarVisibilityL(
        CEikScrollBarFrame::EOn, CEikScrollBarFrame::EAuto);
    iListBox->SetRect(ClientRect());
    iListBox->SetListBoxObserver(this);
    iListBox->ActivateL();
    
    // Add profiles to the listbox
    CTextListBoxModel *lbm = iListBox->Model();
    lbm->SetItemTextArray(iProfileListArray);
    lbm->SetOwnershipType(ELbmDoesNotOwnItemArray);
    iListBox->HandleItemAdditionL();
    
    // Activate the UI control
    AppUi()->AddToStackL(iListBox);    
    LFPRINT((_L("InitViewL done")));
}


_LIT(KBadChars, "<>:\"/|*?\\");

// Make a profile name a legal filename
void CProfileListView::MakeNameLegal(TDes &aName) {
    TInt len = aName.Length();
    TBuf<10> bad;
    bad = KBadChars;
    for ( TInt i = 0; i < len; i++ ) {
        if ( bad.Locate(aName[i]) != KErrNotFound ) {
            aName[i] = ' ';
        }
    }
    aName.Trim();
    if ( aName.Length() == 0 ) {
        aName = KBlankProfileName;
    }
}


// Make profile name unique. Must already be a legal filename
void CProfileListView::MakeNameUniqueL(TDes &aName) {
    HBufC *fileNameBuf = HBufC::NewLC(KMaxFileName);
    TPtr fileName = fileNameBuf->Des();
    HBufC *newNameBuf = HBufC::NewLC(KMaxFileName);
    TPtr newName = newNameBuf->Des();

    newName = aName;
        
    TBool done = EFalse;
    TInt num = 2;
    while ( !done ) {
        fileName = iProfileDirectory;
        fileName.Append(newName);
        if ( !BaflUtils::FileExists(CEikonEnv::Static()->FsSession(),
                                    fileName) ) {
            // Have a unique name
            aName = newName;
            break;
        }

        // Start adding numbers to the end of the name
        if ( aName.Length() > (KMaxFileName + 14) ) {
            aName = aName.Left(KMaxFileName + 14);
        }
        newName.Format(KUniqueProfileFormat, &aName, num);
        num++;
    }

    CleanupStack::PopAndDestroy(2); //fileNameBuf, newNameBuf;
}


// MEikListBoxObserver::HandleListBoxEventL()
void CProfileListView::HandleListBoxEventL(CEikListBox * /*aListBox*/,
                                           TListBoxEvent aEventType) {
#ifdef PUTTY_SYM3    
    if ( (aEventType == EEventEnterKeyPressed) ||
         (aEventType == EEventItemDoubleClicked) ||
         (aEventType == EEventItemSingleClicked) ) {    
#else
    if ( (aEventType == EEventEnterKeyPressed) ||
         (aEventType == EEventItemDoubleClicked) ) {
#endif    
        HandleCommandL(EPuttyCmdProfileListConnect);
    }
}

    
#ifdef PROFILELISTVIEW_TOOLBAR
// MAknToolbarObserver::OfferToolbarEventL()
void CProfileListView::OfferToolbarEventL(TInt aCommand) {
    HandleCommandL(aCommand);
}
#endif

    
// MPuttyClient::FatalError
void CProfileListView::FatalError(const TDesC &aMessage) {
    CAknNoteDialog* dlg = new(ELeave) CAknNoteDialog();
    dlg->SetTextL(aMessage);
    dlg->ExecuteDlgLD(R_PUTTY_INFO_MESSAGE_DLG);
    User::Exit(KFatalErrorExit);
}

// Dummy implementations for MPuttyEngine methods
void CProfileListView::DrawText(TInt, TInt, const TDesC &, TBool, TBool,
                                TRgb, TRgb) {
}

void CProfileListView::SetCursor(TInt, TInt) {
}

void CProfileListView::ConnectionError(const TDesC &) {
    User::Invariant();
}

void CProfileListView::ConnectionClosed() {
}

MPuttyClient::THostKeyResponse CProfileListView::UnknownHostKey(
    const TDesC &) {
    User::Invariant();
    return EAbadonConnection;
}

MPuttyClient::THostKeyResponse CProfileListView::DifferentHostKey(
    const TDesC &/*aFingerprint*/) {
    User::Invariant();
    return EAbadonConnection;    
}

TBool CProfileListView::AcceptCipher(const TDesC &, const TDesC &) {
    User::Invariant();
    return EFalse;
}

TBool CProfileListView::AuthenticationPrompt(const TDesC &, TDes &, TBool) {
    User::Invariant();
    return EFalse;
}

void CProfileListView::PlayBeep(const TInt /*iMode*/) {
    User::Invariant();
    return;
}

