        PuTTY for Symbian OS
        --------------------

Version 1.5 beta 1, 4 March 2008

Copyright 2002-2008 Petteri Kangaslampi
Portions copyright 2003-2004 Sergei Khloupnov.
Based on PuTTY 0.56 beta, Copyright 1997-2004 Simon Tatham.
See license.txt for full copyright and license information.


Introduction
------------

This package is a fresh beta release of PuTTY SSH client for Symbian
OS based smartphones. As a beta release it is expected to be
reasonably stable, but can still have bugs, missing functionality, and
general rough edges.

The previous beta release, 1.4 beta 1, is now almost two years old,
and there have been a large number of changes since then. To reflect
this, this release has been numbered 1.5 beta 1. For a brief list of
changes, see the "Changes" section below.

PuTTY is distributed in two different packages, one for S60 third
edition, and one for Series 80 phones. Make sure you use the correct
version for your phone model. The packages are:

putty_s60v3_*   S60 third edition, supporting all current S60
                smartphones. Includes Nokia E61, N80, N95 etc
putty_s80v2_*   Series 80 v2.0. Nokia 9300, 9300i, 9500

Note that only S60 third edition and Series 80 v2.0 are
supported. PuTTY 1.4 beta 1 supported a wider range of phones, and
users with earlier S60 phones, a Nokia 9200 series communicator, or a
Nokia 7710 can try it. A separate UIQ v3.x port is available at
        http://coredump.fi/putty

PuTTY is free software, and available with full source code under a
very liberal license agreement.

The user's guide contains further documentation, and has been recently
updated.


Installing PuTTY on Series 80
-----------------------------

PuTTY installation packages for Series 80 are signed with a
self-signed certificate. To be able to verify the packages, you'll
need to install the certificate to the device. The steps needed are:

1. Fetch the certificate from
   http://www.s2.org/~pekangas/petteri_s80_2008_der and unzip it.

2. Verify the certificate. Its MD5 sum is
   652a37da35fb97cde1a31a6c8af040cf. A PGP signature is available at
   http://www.s2.org/~pekangas/petteri_s80_2008_der.cer.asc, the key is
   http://www.s2.org/~pekangas/petteri_pgp_2008.asc. The key is also
   available on OpenPGP key servers, ID E393AD7C.

3. Copy the certificate to a file in the communicator.

4. Open Control panel, select the "Security" group, and from there
   open "Certificate manager".

5. Change to the "Other" tab and find the new certificate, named
   "Petteri Kangaslampi" from the list.

6. Select "View details", select "Trust settings" and enable
   "Application installation"

After installing the certificate, install the .SIS package normally.


Installing PuTTY on S60
-----------------------

PuTTY S60 3rd edition installation packages are self-signed. Many S60
devices, including all Nokia E-series phones, refuse to install
self-signed applications by default.

To enable this, go to the device main menu, select Tools and start
Application Manager. From the application manager press Options,
select Settings, and set Software installation to All. The names will
be different in devices using a different language but the same
setting should be present. This is a mandatory operation, otherwise
PuTTY will not install, and the installer will complain about a
certificate error!

Unfortunately there is no way to add new trusted application signing
keys to an S60 device, so you will need to use PGP signatures to
verify their authentity. All installation packages have a separate PGP
signature. The key is available at
http://www.s2.org/~pekangas/petteri_pgp_2008.asc and on OpenPGP key
servers, ID E393AD7C.

S60 releases are distributed in files named
putty_s60v3_version.zip. Download the version you want, verify its PGP
signature, unzip the file, and transfer the .SISX package from the
archive to your phone. You can then install the package by opening it
from the e.g. the Messages Inbox or using the file manager.


Changes
-------

Main changes since 1.4 beta 1:
- Support for different character encodings, most notably UTF-8
- S80 settings improvements: Settings are stored as profiles, and the
  application presents a profile list at startup.
- S60 third edition bug fixes and additional fonts. PuTTY now works
  properly on an E90
- Refactored and largely rewritten S60 UI, with a separate profile
  list view, proper settings views, no need to exit the application
  after each connection etc
- S60 settings brought up to date with S80
- Replaced the "Send" menu with a new grid control. This avoids problems
  with multiply nested menus that have appeared on recent S60 3.1
  devices, and should also improve usability on non-QWERTY devices
- Lots of bug fixes, UI improvements, and general cleanup

See Changelog in the source distribution for more details.


Future plans
------------

Changes planned for 1.5 final:
- Fix bugs
- Update to the latest PuTTY engine core
- Possibly copy/paste support for S60
- Any good ideas? Contact me, preferably with patches


Contact information
-------------------

The project home page is at
        http://s2putty.sourceforge.net/

The site also contains the source code, news, and other useful
information.

Feedback, bug reports, comments, and questions can be e-mailed to
        pekangas@s2.org

I can't promise to answer all e-mail, but will definitely read it all.

Mailing lists, downloads, and other information can be found at
        http://www.sourceforge.net/projects/s2putty/


Acknowledgements
----------------

This Symbian OS port is partially based on the SyOSsh by Gabor
Keresztfavli. Especially the network code, noise generation, and
storage implementation borrow heavily from SyOSsh.

The original S60 port was written by Sergei Khloupnov.

Obviously this program wouldn't exist without the original PuTTY SSH
client by Simon Tatham and others. Many thanks for writing such an
excellent application and releasing it as free software!
