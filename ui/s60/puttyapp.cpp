/*    puttyapp.cpp
 *
 * Putty UI Application class
 *
 * Copyright 2003 Sergei Khloupnov
 * Copyright 2002,2006 Petteri Kangaslampi
 *
 * See license.txt for full copyright and license information.
*/

#ifdef EKA2
#include <eikstart.h>
#endif
#include "puttyapp.h"
#include "puttydoc.h"

#ifdef EKA2
const TUid KUidPutty = { 0xf01f9075 };
#else
const TUid KUidPutty = { 0x101f9075 };
#endif

#ifndef PUTTY_S60
#error Symbol PUTTY_S60 not defined -- build environment is incorrect
#endif

TUid CPuttyApplication::AppDllUid() const {
    return KUidPutty;
}

CApaDocument* CPuttyApplication::CreateDocumentL() {
    return new (ELeave) CPuttyDocument(*this);
}


// Application entry point
EXPORT_C CApaApplication *NewApplication() {
    return new CPuttyApplication;
}

#ifdef EKA2

// EXE point
GLDEF_C TInt E32Main() {
    return EikStart::RunApplication( NewApplication );
}

#else

// DLL entry point
GLDEF_C TInt E32Dll(TDllReason /*aReason*/) {
    return KErrNone;
}

#endif
