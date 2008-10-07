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

  :SET OPTIONS=%OPTIONS% --create-filter=trace,ser --add-filters=0:ser

  SET OPTIONS=%OPTIONS% --create-filter=escparse,escparse-com --add-filters=0:escparse-com
  SET OPTIONS=%OPTIONS% --create-filter=pinmap:"--rts=cts --dtr=dsr --break=break" --add-filters=0:pinmap
  SET OPTIONS=%OPTIONS% --create-filter=linectl --add-filters=0:linectl

  :SET OPTIONS=%OPTIONS% --create-filter=trace,tcp --add-filters=1:tcp

  SET OPTIONS=%OPTIONS% --create-filter=escparse,escparse-tcp:"--request-esc-mode=no" --add-filters=1:escparse-tcp
  SET OPTIONS=%OPTIONS% --create-filter=escinsert --add-filters=1:escinsert

  :SET OPTIONS=%OPTIONS% --create-filter=trace,s2t --add-filters=1:s2t

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
