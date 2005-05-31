# created by RPM Builder for Anjuta, v0.1.2
# http://arpmbuilder.sourceforge.net
# Wed Feb 25 12:14:17 2004

%define _prefix	/usr

Summary:         %{name}
Name:            libkmfl
Version:         0.6
Release:         1suse
Vendor:          SIL <doug_rintoul@sil.org>
Packager:        Doug Rintoul <doug_rintoul@sil.org>
Group:           User Interface/X
License:         GPL
Source0:         %{name}-%{version}.suse.tar.gz
# Url:             (none)
BuildRoot:       /var/tmp/libkmfl
BuildArch:       i586
Requires:        kmflcomp
Buildrequires:   kmflcomp-devel
# Conflicts:       (none)
# Provides:        (none)
# Obsoletes:       (none)

%description 
Keystroke interpreter for KMFL

%package devel
Summary:         Development libraries and headers to use %{name} in an application
Group:           Development/Libraries
Requires:        kmflcomp
Buildrequires:   kmflcomp-devel
# Conflicts:       (none)
# Provides:        (none)
# Obsoletes:       (none)

%description devel
Keystroke interpreter for KMFL

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
	rm -f $RPM_BUILD_ROOT%{_prefix}/doc/libkmfl/$doc;
done;

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files 
%defattr(-,root,root)
%{_libdir}/libkmfl.so.0.0.0
%{_libdir}/libkmfl.so.0

%doc AUTHORS COPYING ChangeLog README INSTALL NEWS TODO

%files devel
%defattr(-,root,root)
%{_includedir}/kmfl/libkmfl.h
%{_libdir}/libkmfl.la
%{_libdir}/libkmfl.a
%{_libdir}/libkmfl.so

