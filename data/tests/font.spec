Summary:	A test font
Name:		font
Version:	1
Release:	1%{?dist}
URL:		http://people.freedesktop.org/
License:	GPLv2+
BuildArch:	noarch

Source0:	COPYING
Source1:	fonts.dir
Source2:	fonts.scale
Source3:	LiberationSerif-Bold.ttf
Source4:	LiberationSerif-Regular.ttf
Source5:	Liberation.metainfo.xml
Source6:	LiberationSerif.metainfo.xml

%description
This is a test font.

%package serif
Summary:	Serif fonts
Requires:	%{name}%{?_isa} = %{version}-%{release}
BuildArch:	noarch

%description serif
Test serif fonts.

%install
install -Dp %{SOURCE0} $RPM_BUILD_ROOT/%{_datadir}/liberation/COPYING
install -Dp %{SOURCE1} $RPM_BUILD_ROOT/%{_datadir}/fonts/liberation/fonts.dir
install -Dp %{SOURCE2} $RPM_BUILD_ROOT/%{_datadir}/fonts/liberation/fonts.scale
install -Dp %{SOURCE3} $RPM_BUILD_ROOT/%{_datadir}/fonts/liberation/LiberationSerif-Bold.ttf
install -Dp %{SOURCE4} $RPM_BUILD_ROOT/%{_datadir}/fonts/liberation/LiberationSerif-Regular.ttf
install -Dp %{SOURCE5} $RPM_BUILD_ROOT/%{_datadir}/appdata/Liberation.metainfo.xml
install -Dp %{SOURCE6} $RPM_BUILD_ROOT/%{_datadir}/appdata/LiberationSerif.metainfo.xml

%files
%defattr(-,root,root)
%{_datadir}/liberation/COPYING
%{_datadir}/fonts/liberation/fonts.dir
%{_datadir}/fonts/liberation/fonts.scale
%{_datadir}/appdata/Liberation.metainfo.xml

%files serif
%{_datadir}/fonts/liberation/LiberationSerif-Bold.ttf
%{_datadir}/fonts/liberation/LiberationSerif-Regular.ttf
%{_datadir}/appdata/LiberationSerif.metainfo.xml

%changelog
* Tue Aug 12 2014 Richard Hughes <richard@hughsie.com> - 1-1
- Initial version
