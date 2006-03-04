# created by RPM Builder for Anjuta, v0.1.2
# http://arpmbuilder.sourceforge.net
# Wed Feb 25 12:58:05 2004

%define _prefix	/usr
%define _unpackaged_files_terminate_build 0

%define is_mandrake %(test -e /etc/mandrake-release && echo 1 || echo 0)
%define is_mandriva %(test -e /etc/mandriva-release && echo 1 || echo 0)
%define is_suse %(test -e /etc/SuSE-release && echo 1 || echo 0)
%define is_fedora %(test -e /etc/fedora-release && echo 1 || echo 0)

%if %is_mandrake
%define dist mandrake
%define disttag mdk
%endif
%if %is_mandriva
%define dist mandriva
%define disttag mdk
%endif
%if %is_suse
%define dist suse
%define disttag suse
%define kde_path /opt/kde3
%endif
%if %is_fedora
%define dist fedora
%define disttag rhfc
%endif

%define distver %(release="`rpm -q --queryformat='%{VERSION}' %{dist}-release 2> /dev/null | tr . : | sed s/://g`" ; if test $? != 0 ; then release="" ; fi ; echo "$release")

Summary:         %{name}
Name:            scim_kmfl_imengine
Version:         0.9.2
Release:         1%{disttag}%{distver}
Vendor:          SIL <doug_rintoul@sil.org>
Packager:        Doug Rintoul <doug_rintoul@sil.org>
Group:           Applications/System
License:         GPL
Source0:         %{name}-%{version}.tar.gz
# Url:             (none)
BuildRoot:       /var/tmp/scim_kmfl_imengine
BuildArch:       i586
Requires:        libkmfl scim >= 1.2.2
Buildrequires:   libkmfl-devel scim-devel
# Conflicts:       (none)
# Provides:        (none)
# Obsoletes:       (none)

%description 
KMFL imengine for SCIM

%package devel
Summary:         Static libraries and headers for %{name}
Group:           Development/Libraries
# Requires:        (none)
# Buildrequires:   (none)
# Conflicts:       (none)
# Provides:        (none)
# Obsoletes:       (none)

%description devel
KMFL imengine for SCIM

%prep
%setup -q

%build
[ ! -f Makefile ] || make distclean
%configure
make

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

for doc in ABOUT-NLS AUTHORS README COPYING INSTALL NEWS TODO ChangeLog; do
	rm -f $RPM_BUILD_ROOT%{_prefix}/doc/scim_kmfl_imengine/$doc;
done;

make DESTDIR=${RPM_BUILD_ROOT} install-strip

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files 
%defattr(-,root,root)
%{_libdir}/scim-1.0/*/IMEngine/kmfl.so
%{_libdir}/scim-1.0/*/SetupUI/kmfl_imengine_setup.so

%doc AUTHORS COPYING ChangeLog README INSTALL NEWS TODO

%files devel
%defattr(-,root,root)
%{_libdir}/scim-1.0/*/IMEngine/kmfl.la
%{_libdir}/scim-1.0/*/IMEngine/kmfl.a
%{_libdir}/scim-1.0/*/SetupUI/kmfl_imengine_setup.a
%{_libdir}/scim-1.0/*/SetupUI/kmfl_imengine_setup.la
