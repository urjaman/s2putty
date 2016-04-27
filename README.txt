        PuTTY for Symbian OS
        --------------------

Version 1.4 Beta 1, 5 March 2006

Copyright 2002-2006 Petteri Kangaslampi
Based on PuTTY 0.56 beta, Copyright 1997-2004 Simon Tatham.
See license.txt for full copyright and license information.


Introduction
------------

This package contains a beta release of PuTTY SSH Client. As a beta
release it is expected to be reasonably stable, but can still have
bugs, missing functionality, and general rough edges.

PuTTY is distributed in several packages for different Symbian OS
variants. Make sure you use the correct version for your phone
model. The packages are:

putty_s60v1_*   S60 first edition. Nokia 7650, 3650, N-gage, ...
putty_s60v2_*   S60 second edition. Nokia 6600, 6630, N70, ...
putty_s60v3_*   S60 third edition. Nokia E61, N80, ...
putty_s80v1_*   Series 80 v1.0. Nokia 9200 communicator series
putty_s80v2_*   Series 80 v2.0. Nokia 9300, 9300i, 9500
putty_s90_*     Series 90. Nokia 7710

PuTTY is free software, and available with full source code under a
very liberal license agreement.

The user's guide contains further documentation. It has not been
updated for v1.4, but is mostly valid. Most of the 9200 information
also applies to Series 80 v2.0.


Package signatures for S80
--------------------------

Series 80 installation packages originating from me are signed using a
self-signed certificate. To be able to verify the packages, you'll
need to install the certificate to the device. The steps needed are:

1. Fetch the certificate from 
   http://www.s2.org/~pekangas/petteri_kangaslampi_2006.cer.zip
   and unzip it.
2. Verify the certificate. Its MD5 sum is
   58761b041e57cfa75f659b2bc5389894. A PGP signature is available at
   http://www.s2.org/~pekangas/petteri_kangaslampi_2006.asc, the key is
   http://www.s2.org/~pekangas/pekangas.public.asc. The key is also
   available at the MIT keyserver, http://pgp.mit.edu/ (ID 2A5111C9).
3. Copy the certificate to a file in the communicator.
4. Open Certificate manager from the communicator's Control panel.
5. Select "Add" and choose the file
6. Select the certificate from the list, select "View details", 
   select "Trust settings" and enable "Software installation"


Package signatures for other platforms
--------------------------------------

S60 and Series 90 installation packages are not signed, since most S60
devices cannot import user-created sertificats. All releases are
PGP-signed however. The key is available at
         http://www.s2.org/~pekangas/pekangas.public.asc.
The key is also available at the MIT keyserver, http://pgp.mit.edu/
(ID 2A5111C9).


Changes
-------

Main changes since 1.3.2:
- Support for S60 third edition, Series 80 2.0, and Series 90
- IPV6 support on most platforms (Not S60 first edition, S80 1.0)
- Copy/paste support on Series 80
- Rewritten text output and font system on S60. This should fix all
  font-related problems, including bugs 1333569, 1037049, and 887459
- Added compression and keepalive options to S80 settings. Note that
  compression is enabled by default on all platforms, but on S80 it
  can now be disabled.
- Multitap input directly to the terminal on S60. T9 unfortunately
  isn't supported and may never be, since it doesn't behave well with
  terminal-style UI controls.
- Reorganized source code tree to better support all the different
  platforms.
- Various small bug fixes and general cleanup.

See Changelog in the source distribution for more details.


Future plans
------------

Changes planned for 1.4 final:
- Add proper connection establishment code for 7.0s+ based platforms.
- Further UI cleanup
- Fix bugs
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

Obviously this program wouldn't exist without the original PuTTY SSH
client by Simon Tatham and others. Many thanks for writing such an
excellent application and releasing it as free software!
