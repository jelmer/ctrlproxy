CtrlProxy for Windows!
----------------------
This directory contains some files required to compile CtrlProxy for Windows 
using Microsoft Visual C++. Required dependencies are: glib, glib-dev, zlib, 
gettext, libintl, iconv from ethereal.com. libxml from http://www.zlatkovic.com/pub/libxml/. 
For the python module, the standard python install from python.org and libxml2 bindings from 
http://users.skynet.be/sbi/libxml-python/ are required.

Working modules:
----------------
admin
antiflood
autosave
auto_away
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
socket

Modules that need work:
-----------------------
python: compiles, but doesn't run yet.
dcc: needs tcp update
stats: will get rewrite in 2.8

Other TODO stuff:
-----------------
- Finish network config dialog
- Finish plugin config dialog
- Finish status dialog
- Icon!