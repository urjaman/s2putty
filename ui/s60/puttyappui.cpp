/*    puttyappui.cpp
 *
 * Putty UI Application UI class
 *
 * Copyright 2003-2004 Sergei Khloupnov
 * Copyright 2002-2004,2007,2010 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#include <e32std.h>
#include <bautils.h>
#include <aknnavi.h>
#include <badesca.h>
#include "puttyappui.h"
#include "profilelistview.h"
#include "terminalview.h"
#include "puttyuids.hrh"
#include "../common/logfile.h"

#ifdef PUTTY_S60V2
_LIT(KFontDirFormat, "%c:\\system\\apps\\putty\\fonts\\");
_LIT(KProfileDirFormat, "%c:\\system\\apps\\putty\\profiles\\");
_LIT(KDataDirFormat,  "%c:\\system\\apps\\putty\\data\\");
_LIT(KSettingsDirFormat, "%c:\\system\\apps\\putty\\settings\\");

#else
_LIT(KFontDirFormat, "%c:\\resource\\puttyfonts\\");
_LIT(KProfileDirFormat, "%c:\\private\\%08x\\profiles\\");
_LIT(KSettingsDirFormat, "%c:\\private\\%08x\\settings\\");
_LIT(KDataDirFormat,  "%c:\\private\\%08x\\data\\");
#endif

// Previous PuTTY versions kept setting and host key files at
// "c:\system\apps\putty". We'll try to migrate them to the new locations.
_LIT(KOldHostKeysFile, "c:\\system\\apps\\putty\\hostkeys.dat");
_LIT(KNewDefaultProfileFile, "Default");
_LIT(KNewHostKeysFile, "hostkeys.dat");

// Second-phase constructor
void CPuttyAppUi::ConstructL() {
#ifdef PUTTY_S60V3
    #ifdef PUTTY_SYM3
        BaseConstructL(CAknAppUi::EAknEnableSkin | EAknTouchCompatible | EAknSingleClickCompatible);
    #else
        BaseConstructL(CAknAppUi::EAknEnableSkin);
    #endif
#else
    BaseConstructL();
#endif

    // Determine profile, data and font directories based on the executable
    // installation location. The files are on the same drive as the
    // executable, except if the exe is in ROM (z:), in which case profiles and
    // data use c:.
    TFileName name;
    name = RProcess().FileName();
    TParse parsa;
    parsa.SetNoWild(name, NULL, NULL);
    TUint drive = parsa.Drive()[0];

    // Font directory -- "<drv>:\resource\puttyfonts\"
    iFontDirectory.Format(KFontDirFormat, drive);
    LFPRINT((_L("Hello, procname:%S fontdir:%S"), &name, &iFontDirectory));

    // Fix drive for profiles and data
    if ( (drive == 'z') || (drive == 'Z') ) {
        drive = 'c';
    }

    RFs &fs = CEikonEnv::Static()->FsSession();
    // Try C if it wasnt in Z...
    if ( !BaflUtils::FolderExists(fs, iFontDirectory) ) {
	iFontDirectory.Format(KFontDirFormat, drive);
        BaflUtils::EnsurePathExistsL(fs, iFontDirectory);
	LFPRINT((_L("changed fontdir to :%S"), &iFontDirectory));
    }

    // Data directory -- "<drv>:\private\<SID>\data\"
    // If the data directory doesn't exist, create it and attempt to migrate
    // host keys from a previous installation
#ifdef PUTTY_S60V2
    iDataDirectory.Format(KDataDirFormat, drive);
    iProfileDirectory.Format(KProfileDirFormat, drive);
    iSettingsDirectory.Format(KSettingsDirFormat, drive);
#else
    iDataDirectory.Format(KDataDirFormat, drive, RProcess().SecureId().iId);
    iProfileDirectory.Format(KProfileDirFormat, drive, RProcess().SecureId().iId);
    iSettingsDirectory.Format(KSettingsDirFormat, drive, RProcess().SecureId().iId);
#endif
    if ( !BaflUtils::FolderExists(fs, iDataDirectory) ) {
        BaflUtils::EnsurePathExistsL(fs, iDataDirectory);
        if ( BaflUtils::FileExists(fs, KOldHostKeysFile) ) {
            name = iDataDirectory;
            name.Append(KNewHostKeysFile);
            BaflUtils::CopyFile(fs, KOldHostKeysFile, name);
        }
    }
    LFPRINT((_L("iDataDir done")));

    // Profile directory -- "<drv>:\private\<SID>\profiles\"
    // If the profile directory doesn't exist, create it.
    BaflUtils::EnsurePathExistsL(fs, iProfileDirectory);

    LFPRINT((_L("iProfileDir created")));

    // Settings directory -- "<drv>:\private\<SID>\settings\"
    // If the profile directory doesn't exist, create it and attempt to migrate
    // default settings from a previous installation
    BaflUtils::EnsurePathExistsL(fs, iSettingsDirectory);

    LFPRINT((_L("iSettingsdir created")));

    // Create navi pane
    iNaviPane = (CAknNavigationControlContainer*)
        (StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidNavi)));

    LFPRINT((_L("navi pane created")));

    // Build a list of available fonts
    iFonts = new CDesC16ArrayFlat(8);
    CDir *dir;
    User::LeaveIfError(
        CEikonEnv::Static()->FsSession().GetDir(iFontDirectory,
                                                KEntryAttNormal,
                                                ESortByName, dir));
    CleanupStack::PushL(dir);
    for ( TInt i = 0; i < dir->Count(); i++ ) {
        parsa.SetNoWild((*dir)[i].iName, NULL, NULL);
        iFonts->AppendL(parsa.Name());
    }
    CleanupStack::PopAndDestroy(); //dir    
    LFPRINT((_L("Fonts parsed\n")));

    // Build views
    iProfileListView = CProfileListView::NewL();
    AddViewL(iProfileListView);
    LFPRINT((_L("ProfileList\n")));

    iProfileEditView = CProfileEditView::NewL();
    AddViewL(iProfileEditView);
    LFPRINT((_L("ProfileEdit\n")));

    iTerminalView = CTerminalView::NewL();
    AddViewL(iTerminalView);
    LFPRINT((_L("TerminalView\n")));

    // Start from the profile list view.
    SetDefaultViewL(*iProfileListView);
    LFPRINT((_L("Init doone\n")));
}


// Destructor
CPuttyAppUi::~CPuttyAppUi() {
    delete iFonts;
}


// Handle menu commands forwarded from views
void CPuttyAppUi::HandleCommandL(TInt aCommand) {

    switch (aCommand) {

        case EEikCmdExit:
        case EAknSoftkeyExit:
            // Exit
            Exit();
            break;
            
        default:
            break;
            User::Invariant();
    }    
}


// Activate profile list view
void CPuttyAppUi::ActivateProfileListViewL() {
    ActivateLocalViewL(TUid::Uid(KUidPuttyProfileListViewDefine));    
}


// Activate profile edit view
void CPuttyAppUi::ActivateProfileEditViewL(CPuttyEngine &aPutty,
                                           TDes &aProfileName,
                                           TTouchSettings &aSettings) {
    iProfileEditPutty = &aPutty;
    iProfileEditName = &aProfileName;
    iProfileEditSettings = &aSettings;
    ActivateLocalViewL(TUid::Uid(KUidPuttyProfileEditViewDefine));    
}


// Get profile edit view data
void CPuttyAppUi::GetProfileEditDataL(CPuttyEngine *&aPutty,
                                      TDes *&aProfileName,
                                      TTouchSettings *&aSettings) {
    aPutty = iProfileEditPutty;
    aProfileName = iProfileEditName;
    aSettings = iProfileEditSettings;
}


// Activate terminal view
void CPuttyAppUi::ActivateTerminalViewL(const TDesC &aProfileFile,
                                        const TDesC &aSettingsFile) {
    iTerminalProfileFile = aProfileFile;
    iTerminalSettingsFile = aSettingsFile;
    ActivateLocalViewL(TUid::Uid(KUidPuttyTerminalViewDefine));
}


// Get connection profile file
const TDesC &CPuttyAppUi::TerminalProfileFile() {
    return iTerminalProfileFile;
}

// Get connection settings file
const TDesC &CPuttyAppUi::TerminalSettingsFile() {
    return iTerminalSettingsFile;
}


// Get profile directory
const TDesC &CPuttyAppUi::ProfileDirectory() {
    return iProfileDirectory;
}

// Get settings directory
const TDesC &CPuttyAppUi::SettingsDirectory() {
    return iSettingsDirectory;
}

// Get data directory
const TDesC &CPuttyAppUi::DataDirectory() {
    return iDataDirectory;
}

// Get font directory
const TDesC &CPuttyAppUi::FontDirectory() {
    return iFontDirectory;
}


// Get navi pane
CAknNavigationControlContainer &CPuttyAppUi::NaviPane() {
    return *iNaviPane;
}


// Get fonts
const CDesCArray &CPuttyAppUi::Fonts() {
    return *iFonts;
}

#ifdef PUTTY_SYM3 //partial screen vkb
//Handle for splitview vkb
void CPuttyAppUi::HandleResourceChangeL( TInt aType ) {
    CAknViewAppUi::HandleResourceChangeL( aType );
    
    switch (aType) {
        case KAknSplitInputEnabled:
            if ( iTerminalView ) {
                iTerminalView->SetHalfKB(ETrue, ClientRect());
            }
            break;
        case KAknSplitInputDisabled:
            if ( iTerminalView ) {
                iTerminalView->SetHalfKB(EFalse, ClientRect());
            }
            if ( iProfileListView ) {
                iProfileListView->HandleStatusPaneSizeChange(); // Make redraw to get correct rect.
            }
            if ( iProfileEditView ) {
                iProfileEditView->HandleStatusPaneSizeChange(); // Make redraw to get correct rect.
            }
            break;
        default:
            return;
    }    
}
#endif
