@ECHO OFF

SETLOCAL
  IF DEFINED HUB4COM GOTO DEFINED_HUB4COM
    SET HUB4COM=hub4com
  :DEFINED_HUB4COM

  PATH %~dp0;%PATH%

  SET PERMANENT=*
  SET TC=:

  :BEGIN_PARSE_OPTIONS
    SET OPTION=%~1
    IF NOT "%OPTION:~0,2%" == "--" GOTO END_PARSE_OPTIONS
    SHIFT /1

    IF /I "%OPTION%" == "--help" GOTO USAGE

    IF /I "%OPTION%" NEQ "--trace" GOTO END_OPTION_TRACE
      SET TC=
      GOTO BEGIN_PARSE_OPTIONS
    :END_OPTION_TRACE

    IF /I "%OPTION%" NEQ "--interface" GOTO END_OPTION_INTERFACE
      SET OPTIONS=%OPTIONS% --interface=%~1
      SHIFT /1
      GOTO BEGIN_PARSE_OPTIONS
    :END_OPTION_INTERFACE

    IF /I "%OPTION%" NEQ "--rfc2217-mode" GOTO END_OPTION_RFC2217_MODE
      SET ARG=%~1
      SHIFT /1

      IF /I "%ARG:~0,1%" NEQ "c" GOTO END_OPTION_RFC2217_MODE_CLIENT
        SET LC_CLIENT_MODE=yes
        GOTO BEGIN_PARSE_OPTIONS
      :END_OPTION_RFC2217_MODE_CLIENT
      IF /I "%ARG:~0,1%" NEQ "s" GOTO END_OPTION_RFC2217_MODE_SERVER
        SET LC_CLIENT_MODE=no
        GOTO BEGIN_PARSE_OPTIONS
      :END_OPTION_RFC2217_MODE_SERVER
      GOTO USAGE
    :END_OPTION_RFC2217_MODE

    IF /I "%OPTION%" NEQ "--connect" GOTO END_OPTION_CONNECT
      SET COM_PIN2CON=--create-filter=pin2con,com,connect:%~1
      SET PERMANENT=
      SHIFT /1
      GOTO BEGIN_PARSE_OPTIONS
    :END_OPTION_CONNECT

    IF /I "%OPTION%" NEQ "--keep-active" GOTO END_OPTION_KEEP_ACTIVE
      SET TCP_TELNET_OPTIONS=%TCP_TELNET_OPTIONS% --keep-active=%~1
      SHIFT /1
      GOTO BEGIN_PARSE_OPTIONS
    :END_OPTION_KEEP_ACTIVE

    GOTO USAGE
  :END_PARSE_OPTIONS

  :BEGIN_PARSE_ARGS
    IF "%~1" == "" GOTO USAGE
    SET COMPORT=%~1
    SHIFT /1

    IF "%~1" == "" GOTO USAGE
    SET TCP=%~1
    SHIFT /1

    IF "%~1" == "" GOTO END_PARSE_ARGS
    SET TCP=%TCP%:%~1
    SHIFT /1

    IF /I "%LC_CLIENT_MODE%" == "no" GOTO END_SET_LC_CLIENT_MODE
      SET LC_CLIENT_MODE=yes
    :END_SET_LC_CLIENT_MODE

    IF NOT "%~1" == "" GOTO USAGE
  :END_PARSE_ARGS

  IF /I "%LC_CLIENT_MODE%" == "yes" GOTO SET_LC_CLIENT_MODE_OPTIONS

    SET TCP_TELNET_OPTIONS=%TCP_TELNET_OPTIONS% --comport=server --suppress-echo=yes
    SET TCP_LSRMAP=--create-filter=lsrmap,tcp,lsrmap
    SET TCP_PINMAP_OPTIONS=:"--cts=cts --dsr=dsr --dcd=dcd --ring=ring"
    SET TCP_LC_OPTIONS=:"--br=local --lc=local"

    SET COM_PINMAP_OPTIONS=:"--rts=cts --dtr=dsr --break=break"
    SET COM_LC_OPTIONS=:"--br=remote --lc=remote"
    SET COM_PURGE=--create-filter=purge,com,purge

    GOTO END_SET_LC_MODE_OPTIONS
  :SET_LC_CLIENT_MODE_OPTIONS

    SET TCP_TELNET_OPTIONS=%TCP_TELNET_OPTIONS% --comport=client
    SET TCP_PINMAP_OPTIONS=:"--rts=cts --dtr=dsr --break=break"
    SET TCP_LC_OPTIONS=:"--br=remote --lc=remote"
    :SET TCP_PURGE=--create-filter=purge,tcp,purge

    SET COM_PINMAP_OPTIONS=:"--rts=cts --dtr=dsr"
    SET COM_LC_OPTIONS=:"--br=local --lc=local"

  :END_SET_LC_MODE_OPTIONS

  IF "%TCP_TELNET_OPTIONS%" == "" GOTO END_QUOTE_TCP_TELNET_OPTIONS
    SET TCP_TELNET_OPTIONS=:"%TCP_TELNET_OPTIONS%"
  :END_QUOTE_TCP_TELNET_OPTIONS

  %TC% SET OPTIONS=%OPTIONS% --create-filter=trace,com,COM
  SET OPTIONS=%OPTIONS% --create-filter=escparse,com,parse
  %TC% SET OPTIONS=%OPTIONS% --create-filter=trace,com,ExM
  SET OPTIONS=%OPTIONS% %COM_PURGE%
  SET OPTIONS=%OPTIONS% %COM_PIN2CON%
  SET OPTIONS=%OPTIONS% --create-filter=pinmap,com,pinmap%COM_PINMAP_OPTIONS%
  SET OPTIONS=%OPTIONS% --create-filter=linectl,com,lc%COM_LC_OPTIONS%
  %TC% SET OPTIONS=%OPTIONS% --create-filter=trace,com,CxT

  SET OPTIONS=%OPTIONS% --add-filters=0:com

  %TC% SET OPTIONS=%OPTIONS% --create-filter=trace,tcp,TCP
  SET OPTIONS=%OPTIONS% --create-filter=telnet,tcp,telnet%TCP_TELNET_OPTIONS%
  %TC% SET OPTIONS=%OPTIONS% --create-filter=trace,tcp,TxM
  SET OPTIONS=%OPTIONS% %TCP_PURGE%
  SET OPTIONS=%OPTIONS% %TCP_LSRMAP%
  SET OPTIONS=%OPTIONS% --create-filter=pinmap,tcp,pinmap%TCP_PINMAP_OPTIONS%
  SET OPTIONS=%OPTIONS% --create-filter=linectl,tcp,lc%TCP_LC_OPTIONS%

  SET OPTIONS=%OPTIONS% --add-filters=1:tcp

  @ECHO ON
    "%HUB4COM%" %OPTIONS% --octs=off "%COMPORT%" --use-driver=tcp "%PERMANENT%%TCP%"
  @ECHO OFF
ENDLOCAL

GOTO END
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:USAGE

ECHO Usage (TCP client mode):
ECHO     %0 [options] \\.\^<com port^> ^<host addr^> ^<host port^>
ECHO.
ECHO Usage (TCP server mode):
ECHO     %0 [options] \\.\^<com port^> ^<listen port^>
ECHO.
ECHO Common options:
ECHO     --rfc2217-mode ^{c^|s^}  - use RFC 2217 (c)lient or (s)erver mode (default is
ECHO                             c for TCP client mode and s for TCP server mode).
ECHO     --connect ^[^!^]^<state^>  - connect or disconnect on ^<state^> changing. Where
ECHO                             ^<state^> is cts, dsr, dcd, ring or break. The
ECHO                             exclamation sign ^(^!^) can be used to invert the
ECHO                             action. By default the connection will be permanent
ECHO                             as it's possible.
ECHO     --keep-active ^<s^>     - send NOP command every ^<s^> seconds to keep the
ECHO                             connection active if data is not transferred.
ECHO     --trace               - enable trace output.
ECHO     --help                - show this help.
ECHO.
ECHO TCP client mode options:
ECHO     --interface ^<if^>      - use interface ^<if^> for connecting.
ECHO.
ECHO TCP server mode options:
ECHO     --interface ^<if^>      - use interface ^<if^> for listening.

GOTO END
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:END
