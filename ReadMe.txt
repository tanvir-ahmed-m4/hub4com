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

Start Microsoft Visual C++ 2005 with hub4com.sln file.
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


EXAMPLES OF USAGE
=================

You have a GPS device that connected to your computer via a phisical COM1
port and you'd like to handle its data by two GPS applications. You can
do it this way:

  1. With the com0com's Setup Command Prompt create COM5<->CNCB0 and
     COM6<->CNCB1 virtual COM port pairs (see com0com's ReadMe.txt for
     more info). For example:

       command> install 0 PortName=COM5 -
       command> install 1 PortName=COM6 -

  2. Start the hub4com.exe on COM1, CNCB0 and CNCB1 ports:

       hub4com \\.\COM1 \\.\CNCB0 \\.\CNCB1

     It will send data received from COM1 port to CNCB0 and CNCB1 ports
     and it will send data received from CNCB0 port to COM1 port.

  3. Start the GPS applications on COM5 and COM6 ports.

---

You have old TERM95.EXE application from the Norton Commander 5.0 for
MS-DOS and you'd like to use it to communicate with your.telnet.server
telnet server. You can do so this way:

  1. With the com0com's Setup Command Prompt create COM2<->CNCB0 virtual
     COM port pair (see com0com's ReadMe.txt for more info). For example:

       command> install 0 PortName=COM2 -

  2. Start the com2tcp.bat on CNCB0 port. For example:

       com2tcp --telnet \\.\CNCB0 your.telnet.server telnet

  3. Start the TERM95.EXE on COM2 port.

BTW: com2tcp.bat is a wrapper to hub4com.exe. It works very similar to
     the "COM port to TCP redirector" (com2tcp). It supports all
     com2tcp's options.

---

You have a phisical COM1 port and you'd like to share it through the
network by the RFC 2217 "Telnet Com Port Control Option" protocol.

  1. Start the com2tcp-rfc2217.bat on COM1 port. For example:

       com2tcp-rfc2217 COM1 7000

  It will listen TCP/IP port 7000 for incaming connections and
  redirect them to COM1 port.

---

On the first computer your.comport.server you have a phisical serial port
shared through the network by the RFC 2217 protocol and you'd like to use
it like a virtual serial port on the second computer.

  1. With the com0com's Setup Command Prompt create COM5<->CNCB0 virtual
     COM port pair (see com0com's ReadMe.txt for more info). For example:

       command> install 0 PortName=COM5 -

  2. Start the com2tcp-rfc2217.bat on CNCB0 port. For example:

       com2tcp-rfc2217 \\.\CNCB0 your.comport.server 7000

  It will redirect virtual serial port COM5 on the second computer to the
  phisical serial port on the first computer.
