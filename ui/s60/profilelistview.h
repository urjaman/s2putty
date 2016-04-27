/*    profilelistview.h
 *
 * Putty profile list view
 *
 * Copyright 2007 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifndef PROFILELISTVIEW_H
#define PROFILELISTVIEW_H

#if defined(PUTTY_S60TOUCH) && !defined(PUTTY_SYM3)
#define PROFILELISTVIEW_TOOLBAR
#endif

#include <aknview.h>
#include <eiklbo.h>
#ifdef PROFILELISTVIEW_TOOLBAR
#include <akntoolbarobserver.h>
#endif
#include "profileeditview.h"
#include "puttyclient.h"

// Forward declarations
class CAknSingleStyleListBox;
class CPuttyEngine;
class TTouchSettings;

/**
 * PuTTY profile list view. The profile list view displays a list of available
 * connection profiles. The user can select a profile to connect to, which
 * activates the terminal view. Additionally, the view supports editing
 * (using the settings view) and deleting profiles.
 */
class CProfileListView : public CAknView, public MPuttyClient,
                         public MEikListBoxObserver
#ifdef PROFILELISTVIEW_TOOLBAR
                       , public MAknToolbarObserver
#endif
{

public:
    /** 
     * Factory method.
     *
     * @return A new CProfileListView instance.
     */
    static CProfileListView *NewL();

    /** 
     * Destructor.
     */
    ~CProfileListView();

public: // From CAknView
    TUid Id() const;
    void HandleCommandL(TInt aCommand);
    void DoActivateL(const TVwsViewId &aPrevViewId, TUid aCustomMessageId,
                     const TDesC8 &aCustomMessage);
    void DoDeactivate();
    void HandleStatusPaneSizeChange();

public: // From MEikListBoxObserver
    void HandleListBoxEventL(CEikListBox *aListBox, TListBoxEvent aEventType);

#ifdef PROFILELISTVIEW_TOOLBAR
public: // From MarkToolbarObserver
    void OfferToolbarEventL(TInt aCommand);
#endif

private: // Constructors
    CProfileListView();
    void ConstructL();

private: // Helper functions
    HBufC *FormatForListboxLC(const TDesC &aProfileFileName);
    HBufC *AbsoluteFileNameLC(const TDesC &aProfileFileName);
    void AppendProfileL(const TDesC &aProfileFileName);
    void InitViewL();
    void MakeNameLegal(TDes &aName);
    void MakeNameUniqueL(TDes &aName);
    HBufC *SettingsFileNameLC(const TDesC &aProfileFileName);
    
private: // From MPuttyEngine -- all dummy implementations except FatalError
    void DrawText(TInt aX, TInt aY, const TDesC &aText, TBool aBold,
                  TBool aUnderline, TRgb aForeground,
                  TRgb aBackground);
    void SetCursor(TInt aX, TInt aY);
    void ConnectionError(const TDesC &aMessage);
    void FatalError(const TDesC &aMessage);
    void ConnectionClosed();
    THostKeyResponse UnknownHostKey(const TDesC &aFingerprint);
    THostKeyResponse DifferentHostKey(const TDesC &aFingerprint);
    TBool AcceptCipher(const TDesC &aCipherName,
                       const TDesC &aCipherType);
    TBool AuthenticationPrompt(const TDesC &aPrompt, TDes &aTarget,
                               TBool aSecret);
    void PlayBeep(const TInt iMode);

private:
    CAknSingleStyleListBox *iListBox;
    CDesCArray *iProfileFileArray; // profile filenames
    CDesCArray *iProfileListArray; // profile names formatted for the listbox
    TBuf<29> iProfileDirectory; // "x:\private\12345678\profiles\"
    TBuf<29> iSettingsDirectory; // "x:\private\12345678\settings\"
    TBuf<25> iDataDirectory; // "x:\private\12345678\data\"
    CPuttyEngine *iPutty;
    TFileName iProfileEditName;
    TFileName iProfileOldName;
    TInt iSelectedItem;
    TTouchSettings *iSettings;
#ifdef PROFILELISTVIEW_TOOLBAR
    CAknToolbar *iToolbar;
#endif
};


#endif
