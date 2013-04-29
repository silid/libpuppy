libpuppy
========

Libpuppy is a librarified version of tool called Puppy, which is used for accessing Topfield DVB tuners (with HD) via its USB link. Libpuppy was born out of the need to do some small utilities for the device and finding that puppy almost already is a library, even if its behavior is exposed only through a command line utility.

Libpuppy is licensed under the GPL version 2.

libpuppy can be retrieved from its git-repository. There is also a tarball available of the first release.. It comes with the original command line utility (modified to support xml-output in directory listings) and a couple of tools, namely puppy-dir and puppy-get.

The librarified version uses environment variable PUPPY to determine which device to open to access the Topfield. On my machine, I've arranged udev to make a symbolic link /dev/puppy that points to the device, which is the default device name if the environment variable is not set.

Building
--------

Plain "make" will build the original puppy tool. "make libpuppy.a" will build the librarified version. "make puppy-dir puppy-get" will build the two accompanied utilities.

Utilities
---------

Puppy-get is a tool for retrieving files from the device with wildcard support and with the option to remove the file from the device upon a successful transfer.

Puppy-dir is a tool for listing directory contents. It has support for sorting files in a way that takes into account the naming conventions Topfield uses.
