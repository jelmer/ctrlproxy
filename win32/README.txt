CtrlProxy for Windows!
----------------------
This directory contains some files required to compile CtrlProxy for Windows 
using Microsoft Visual C++. Required dependencies are: glib, glib-dev, zlib, 
gettext, libintl, iconv from ethereal.com. libxml from ???

Working modules:
----------------
admin
autosave
ctcp
debug
linestack_file
linestack_memory
log_custom
log_irssi
motd_file
nickserv
noticelog
python
repl_command
repl_lastdisconnect
repl_limited
repl_memory
repl_none
repl_simple
report_time
strip

Modules that need work:
-----------------------
antiflood: timer with milisecond support update
auto_away: can't find ctrlproxy.h for some weird reason
ctcp: doesn't reply to VERSION yet
dcc: needs tcp update
socket: doesn't compile yet, no ssl/tls support