#!/bin/sh
HOSTNAME=`hostname -s`
FQDN=`hostname -f`
openssl req -new -x509 -days 365 -nodes -out ctrlproxy.pem -keyout ctrlproxy.pem > /dev/null 2>&1 <<+
.
.
.
ControlProxy IRC BNC
$HOSTNAME
$FQDN
root@$HOSTNAME
+
