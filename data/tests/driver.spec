Summary:	A hardware driver
Name:		driver
Version:	1
Release:	1%{?dist}
URL:		http://people.freedesktop.org/
License:	GPLv2+
BuildArch:	noarch

Source6:	driver.metainfo.xml
Source17:	app.png

%description
This is a hardware driver.

%install
install -Dp %{SOURCE6} $RPM_BUILD_ROOT/%{_datadir}/metainfo/driver.metainfo.xml
install -Dp %{SOURCE17} $RPM_BUILD_ROOT%{_datadir}/pixmaps/driver.png

%files
%defattr(-,root,root)
%{_datadir}/metainfo/driver.metainfo.xml
%{_datadir}/pixmaps/driver.png

%changelog
* Tue Jan 30 2018 Richard Hughes <richard@hughsie.com> - 1-1
- Initial version
