@ECHO OFF

SETLOCAL
  IF DEFINED HUB4COM GOTO DEFINED_HUB4COM
    SET HUB4COM=hub4com
  :DEFINED_HUB4COM

  PATH %~dp0;%PATH%

  :BEGIN_PARSE_OPTIONS
    SET OPTION=%1
    IF NOT "%OPTION:~0,2%"=="--" GOTO END_PARSE_OPTIONS
    SHIFT /1

    IF /I "%OPTION%"=="--help" GOTO USAGE

    IF /I "%OPTION%" NEQ "--interface" GOTO END_OPTION_INTERFACE
      SET OPTIONS=%OPTIONS% --interface=%1
      SHIFT /1
      GOTO BEGIN_PARSE_OPTIONS
    :END_OPTION_INTERFACE

    GOTO USAGE
  :END_PARSE_OPTIONS

  :BEGIN_PARSE_ARGS
    IF "%1"=="" GOTO USAGE
    SET COMPORT=%1
    SHIFT /1

    IF "%1"=="" GOTO USAGE
    SET TCP=%1
    SHIFT /1

    IF "%1"=="" GOTO END_PARSE_ARGS
    SET TCP=%TCP%:%1
    SHIFT /1

    IF NOT "%1"=="" GOTO USAGE
  :END_PARSE_ARGS

 :SET OPTIONS=%OPTIONS% --create-filter=trace,com,COM
  SET OPTIONS=%OPTIONS% --create-filter=escparse,com,parse
  SET OPTIONS=%OPTIONS% --create-filter=pinmap,com,pinmap:"--rts=cts --dtr=dsr --break=break"
  SET OPTIONS=%OPTIONS% --create-filter=linectl,com,lc
 :SET OPTIONS=%OPTIONS% --create-filter=trace,com,CxT

  SET OPTIONS=%OPTIONS% --add-filters=0:com

 :SET OPTIONS=%OPTIONS% --create-filter=trace,tcp,TCP
  SET OPTIONS=%OPTIONS% --create-filter=escparse,tcp,parse:"--request-esc-mode=no"
  SET OPTIONS=%OPTIONS% --create-filter=escinsert,tcp,insert
 :SET OPTIONS=%OPTIONS% --create-filter=trace,tcp,ExM
  SET OPTIONS=%OPTIONS% --create-filter=pinmap,tcp,pinmap:"--cts=cts --dsr=dsr --ring=ring --dcd=dcd --break=break"
  SET OPTIONS=%OPTIONS% --create-filter=lsrmap,tcp,lsrmap
  SET OPTIONS=%OPTIONS% --create-filter=linectl,tcp,lc

  SET OPTIONS=%OPTIONS% --add-filters=1:tcp

  @ECHO ON
    "%HUB4COM%" %OPTIONS% --octs=off "%COMPORT%" --use-driver=tcp "*%TCP%"
  @ECHO OFF
ENDLOCAL

GOTO END
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:USAGE

ECHO Usage (client mode):
ECHO     %0 [options] \\.\^<com port^> ^<host addr^> ^<host port^>
ECHO.
ECHO Usage (server mode):
ECHO     %0 [options] \\.\^<com port^> ^<listen port^>
ECHO.
ECHO Common options:
ECHO     --help                - show this help.
ECHO.
ECHO Client mode options:
ECHO     --interface ^<if^>      - use interface ^<if^> for connecting.
ECHO.
ECHO Server mode options:
ECHO     --interface ^<if^>      - use interface ^<if^> for listening.

GOTO END
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:END
