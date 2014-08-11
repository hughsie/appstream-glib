Summary:	A test application
Name:		app
Version:	1
Release:	1%{?dist}
URL:		http://people.freedesktop.org/
License:	GPLv2+
Source0:	README
Source1:	app.desktop
Source2:	app.png
Source3:	app.appdata.xml
Source4:	search-provider.ini
Source5:	index.page
Source6:	app-demo.desktop
Source7:	app.service
Source8:	app.metainfo.xml
Source9:	app-extra.metainfo.xml
Source10:	app.gir
Source11:	en_GB.mo
Source12:	ru.mo
Source13:	app.notifyrc
Source14:	app.bin

%description
This is a test application.

%package extra
Summary:	Extra plugins for app
Requires:	%{name}%{?_isa} = %{version}-%{release}
BuildArch:	noarch

%description extra
Extra plugins to provide new functionality for app.

%install
install -Dp %{SOURCE0} $RPM_BUILD_ROOT/%{_datadir}/%{name}-%{version}/README
install -Dp %{SOURCE1} $RPM_BUILD_ROOT/%{_datadir}/applications/app.desktop
install -Dp %{SOURCE2} $RPM_BUILD_ROOT/%{_datadir}/pixmaps/app.png
install -Dp %{SOURCE3} $RPM_BUILD_ROOT/%{_datadir}/appdata/app.appdata.xml
install -Dp %{SOURCE4} $RPM_BUILD_ROOT/%{_datadir}/gnome-shell/search-providers/search-provider.ini
install -Dp %{SOURCE5} $RPM_BUILD_ROOT/%{_datadir}/help/C/app/index.page
install -Dp %{SOURCE6} $RPM_BUILD_ROOT/%{_datadir}/applications/app-demo.desktop
install -Dp %{SOURCE7} $RPM_BUILD_ROOT/%{_datadir}/dbus-1/services/app.service
install -Dp %{SOURCE8} $RPM_BUILD_ROOT/%{_datadir}/appdata/app.metainfo.xml
install -Dp %{SOURCE9} $RPM_BUILD_ROOT/%{_datadir}/appdata/app-extra.metainfo.xml
install -Dp %{SOURCE10} $RPM_BUILD_ROOT/%{_datadir}/gir-1.0/app.gir
install -Dp %{SOURCE11} $RPM_BUILD_ROOT/%{_datadir}/locale/en_GB/LC_MESSAGES/app.mo
install -Dp %{SOURCE12} $RPM_BUILD_ROOT/%{_datadir}/locale/ru/LC_MESSAGES/app.mo
install -Dp %{SOURCE13} $RPM_BUILD_ROOT/%{_datadir}/kde4/apps/app/app.notifyrc
install -Dp %{SOURCE14} $RPM_BUILD_ROOT/%{_bindir}/app.bin

%files
%defattr(-,root,root)
%doc %{_datadir}/%{name}-%{version}/README
%{_bindir}/app.bin
%{_datadir}/appdata/app.*.xml
%{_datadir}/applications/*.desktop
%{_datadir}/dbus-1/services/app.service
%{_datadir}/gir-1.0/app.gir
%{_datadir}/gnome-shell/search-providers/search-provider.ini
%{_datadir}/help/*/app
%{_datadir}/kde4/apps/app/app.notifyrc
%{_datadir}/locale/en_GB/LC_MESSAGES/app.mo
%{_datadir}/locale/ru/LC_MESSAGES/app.mo
%{_datadir}/pixmaps/app.png

%files extra
%{_datadir}/appdata/app-extra.metainfo.xml

%changelog
* Tue Aug 12 2014 Richard Hughes <richard@hughsie.com> - 1-1
- Initial version
