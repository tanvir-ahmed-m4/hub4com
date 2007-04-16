               ===========================
               HUB for COM ports (hub4com)
               ===========================

INTRODUCTION
============

The HUB for COM ports (hub4com) is a Windows application and is a part
of the com0com project.

It allows to send data received from one COM port to a number of COM
ports and vice versa. In conjunction with the com0com driver the hub4com
makes it possible to handle data from a single serial device by a number
of different applications. For example, several applications can share
data from one GPS device.

The homepage for com0com project is http://com0com.sourceforge.net/.

BUILDING
========

Start Microsoft Visual C++ 2005 with hub4com.vcproj file.
Set Active Configuration to hub4com - Win32 Release.
Build hub4com.exe.

TESTING
=======

  1. With the com0com driver create three virtual COM port pairs with
     port names CNCA0, CNCB0, CNCA1, CNCB1, CNCA2 and CNCB2 (see
     com0com's ReadMe.txt file for details).
  2. Start the hub4com.exe on CNCB0, CNCB1 and CNCB2 ports:

       hub4com --route=All:All \\.\CNCB0 \\.\CNCB1 \\.\CNCB2

  3. Start the HyperTerminal on CNCA0 port.
  4. Start the HyperTerminal on CNCA1 port.
  5. Start the HyperTerminal on CNCA2 port.
  6. The data typed to any HyperTerminal window should be printed on
     the others HyperTerminal windows.


EXAMPLE OF USAGE
================

You have serial device that connected to your computer via COM1 port and
you'd like to handle its data by two applications. You can do it this way:

  1. With the com0com driver create two virtual COM port pairs with
     port names CNCA0, CNCB0, CNCA1 and CNCB1 (see com0com's ReadMe.txt
     for details).

  2. Start the hub4com.exe on COM1, CNCB0 and CNCB1 ports:

       hub4com \\.\COM1 \\.\CNCB0 \\.\CNCB1

     It will send data received from COM1 port to CNCB0 and CNCB1 ports
     and it will send data received from CNCB0 port to COM1 port.

  3. Start the applications on CNCA0 and CNCA1 ports.
