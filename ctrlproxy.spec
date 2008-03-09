Summary: ctrlproxy
Name: ctrlproxy
Version: 2.7
Release: 0
License: GPL
Group: Applications/Internet
Source: http://www.ctrlproxy.org/releases/ctrlproxy-%{version}.tar.gz
Url: http://www.ctrlproxy.org/
Packager: Jelmer Vernooij <jelmer@samba.org>
BuildRoot: /var/tmp/%{name}-buildroot

%description
ctrlproxy is an IRC server with multiserver support. It runs as a daemon
and connects to a number of IRC servers, then allows you to connect from
a workstation and work as the user that is logged in to the IRC server.
After you disconnect, it maintains the connection to the server. It acts
like any normal IRC server, so you can use any IRC client to connect to
it. It supports multiple client connections to one IRC server (under the
same nick), allowing you to connect to IRC using your IRC nick, even
while you have an IRC session open somewhere else. It supports logging
(in the same format as the irssi IRC client), password authentication,
and ctcp (in case no clients are connected).

%prep
%setup -q

%build
%configure
make CC="gcc -I/usr/kerberos/include -I`pwd`"

%install
[ -d "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
%makeinstall \
   mandir=$RPM_BUILD_ROOT/usr/share/man \
   man1dir=$RPM_BUILD_ROOT/usr/share/man/man1 \
   man5dir=$RPM_BUILD_ROOT/usr/share/man/man5 \
   man7dir=$RPM_BUILD_ROOT/usr/share/man/man7 \
   docdir=$RPM_BUILD_ROOT/usr/share/doc/ctrlproxy-%{version} \
   modulesdir=$RPM_BUILD_ROOT/usr/lib/ctrlproxy \
   moddir=$RPM_BUILD_ROOT/usr/lib/ctrlproxy

%clean
[ -d "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc AUTHORS COPYING TODO README ChangeLog
%dir /usr/bin
/usr/bin/*
%dir /usr/share/man
/usr/share/man/*
%dir /usr/lib/ctrlproxy
/usr/lib/ctrlproxy/*
%dir /usr/include
/usr/include/*
%dir /usr/share/ctrlproxy
/usr/share/ctrlproxy/*

%changelog
* Tue Nov 18 2003 Sean Reifschneider <jafo-rpms@tummy.com>
[Release 2.5-1]
- Updated for the 2.5 release.
- Making the "clean" mechanisms safer in the .spec file.
