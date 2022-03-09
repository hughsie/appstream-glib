Summary:	A test application
Name:		app
Version:	1
Release:	1%{?dist}
URL:		http://people.freedesktop.org/
License:	GPLv2+
Source0:	README
Source1:	app.desktop
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
Source15:	console1.desktop
Source16:	console2.desktop
Source17:	app.png
Source18:	app-128x128.png

%description
This is a test application.

%package extra
Summary:	Extra plugins for app
Requires:	%{name}%{?_isa} = %{version}-%{release}
BuildArch:	noarch

%description extra
Extra plugins to provide new functionality for app.

%package console
Summary:	Console application
Requires:	%{name}%{?_isa} = %{version}-%{release}
BuildArch:	noarch

%description console
Sub package with console "application".

%install
install -Dp %{SOURCE0} $RPM_BUILD_ROOT/%{_datadir}/%{name}-%{version}/README
install -Dp %{SOURCE1} $RPM_BUILD_ROOT/%{_datadir}/applications/app.desktop

# test decompressing an absolute symlink destination
install -Dp %{SOURCE17} $RPM_BUILD_ROOT%{_datadir}/app/app-48x48.png
mkdir -p $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/48x48/apps
cd $RPM_BUILD_ROOT
ln -s %{_datadir}/app/app-48x48.png usr/share/icons/hicolor/48x48/apps/app.png
cd -

# test decompressing relative symlink destinations
install -Dp %{SOURCE18} $RPM_BUILD_ROOT%{_datadir}/app/app-128x128.png
mkdir -p $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/128x128/apps
cd $RPM_BUILD_ROOT
# Test links in the same directory as well as outside
ln -s ../../../../app/app-128x128.png usr/share/icons/hicolor/128x128/apps/app-linked.png
ln -s app-linked.png usr/share/icons/hicolor/128x128/apps/app.png
cd -

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
install -Dp %{SOURCE15} $RPM_BUILD_ROOT/%{_datadir}/applications/console1.desktop
install -Dp %{SOURCE16} $RPM_BUILD_ROOT/%{_datadir}/applications/console2.desktop

%files
%defattr(-,root,root)
%doc %{_datadir}/%{name}-%{version}/README
%{_bindir}/app.bin
%{_datadir}/appdata/app.appdata.xml
%{_datadir}/appdata/app.metainfo.xml
%{_datadir}/applications/app-demo.desktop
%{_datadir}/applications/app.desktop
%{_datadir}/dbus-1/services/app.service
%{_datadir}/gir-1.0/app.gir
%{_datadir}/gnome-shell/search-providers/search-provider.ini
%{_datadir}/help/*/app
%{_datadir}/kde4/apps/app/app.notifyrc
%{_datadir}/locale/en_GB/LC_MESSAGES/app.mo
%{_datadir}/locale/ru/LC_MESSAGES/app.mo
%{_datadir}/app/app-48x48.png
%{_datadir}/app/app-128x128.png
%{_datadir}/icons/hicolor/48x48/apps/app.png
%{_datadir}/icons/hicolor/128x128/apps/app-linked.png
%{_datadir}/icons/hicolor/128x128/apps/app.png

%files extra
%{_datadir}/appdata/app-extra.metainfo.xml

%files console
%{_datadir}/applications/console1.desktop
%{_datadir}/applications/console2.desktop

%changelog
* Tue Aug 12 2014 Richard Hughes <richard@hughsie.com> - 1-1
- Initial version
