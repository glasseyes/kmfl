# created by RPM Builder for Anjuta, v0.1.2
# http://arpmbuilder.sourceforge.net
# Mon Feb 23 13:01:55 2004

%define _prefix	/usr

Summary:         %{name}
Name:            kmflcomp
Version:         0.3
Release:         1
Vendor:          SIL <doug_rintoul@sil.org>
Packager:        Doug Rintoul <doug_rintoul@sil.org>
Group:           Applications/System
License:         GPL
Source0:         %{name}-%{version}.tar.gz
# Url:             (none)
BuildRoot:       /var/tmp/kmflcomp
BuildArch:       i386
# Requires:        (none)
Buildrequires:   libkmfl-devel
# Conflicts:       (none)
# Provides:        (none)
# Obsoletes:       (none)

%description 
Compile Keyman-style keyboard layout files to a binary format for use by the KMFL keystroke interpreter.

%prep
%setup -q

%build
[ ! -f Makefile ] || make distclean
%configure
make

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%makeinstall
for doc in ABOUT-NLS AUTHORS README COPYING INSTALL NEWS TODO ChangeLog; do
	rm -f $RPM_BUILD_ROOT%{_prefix}/doc/kmflcomp/$doc;
done;

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files 
%defattr(-,root,root)
%{_bindir}/kmflcomp

%doc AUTHORS COPYING ChangeLog README INSTALL NEWS TODO
