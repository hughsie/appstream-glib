Summary:	A test application
Name:		composite
Version:	1
Release:	1%{?dist}
URL:		http://people.freedesktop.org/
License:	GPLv2+
Source1:	valid1.desktop
Source2:	valid2.desktop

%description
This is a composite application shipping multiple valid apps in one package.

%install
install -Dp %{SOURCE1} $RPM_BUILD_ROOT/%{_datadir}/applications/valid1.desktop
install -Dp %{SOURCE2} $RPM_BUILD_ROOT/%{_datadir}/applications/valid2.desktop

%files
%defattr(-,root,root)
%{_datadir}/applications/valid1.desktop
%{_datadir}/applications/valid2.desktop

%changelog
* Tue Aug 12 2014 Richard Hughes <richard@hughsie.com> - 1-1
- Initial version
