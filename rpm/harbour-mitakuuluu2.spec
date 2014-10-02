Name:       harbour-mitakuuluu2

%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}

Summary:    Mitäkuuluu
Version:    0.8.12
Release:    1
Group:      Qt/Qt
License:    WTFPL
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5
Requires:   nemo-transferengine-qt5 declarative-transferengine-qt5 >= 0.0.44
Requires:   qt5-qtdeclarative-import-sensors qt5-qtsensors-plugin-gestures-sensor qt5-qtsensors-plugin-gestures-shake
Requires:   qt5-qtdeclarative-import-location qt5-qtdeclarative-import-positioning
Requires:   libkeepalive
Requires:   zip
Requires:   dconf
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(sailfishapp)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Sensors)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  desktop-file-utils
BuildRequires:  pkgconfig(nemotransferengine-qt5)
BuildRequires:  gstreamer-devel
BuildRequires:  libcontacts-qt5-devel qtcontacts-sqlite-qt5-extensions
BuildRequires:  libkeepalive-devel
BuildRequires:  qt5-qttools-linguist
BuildRequires:  pkgconfig(dconf)

%description
Mitakuuluu - indie WhatsApp-compatible client for SailfishOS

%prep
%setup -q -n %{name}-%{version}

%build

%qtc_qmake5
%qtc_make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%pre
if /sbin/pidof harbour-mitakuuluu2-server > /dev/null; then
killall harbour-mitakuuluu2-server
fi

if /sbin/pidof harbour-mitakuuluu2 > /dev/null; then
killall harbour-mitakuuluu2
fi

%preun
if /sbin/pidof harbour-mitakuuluu-server > /dev/null; then
killall harbour-mitakuuluu-server
fi

if /sbin/pidof harbour-mitakuuluu2 > /dev/null; then
killall harbour-mitakuuluu2
fi

%post
systemctl-user restart ngfd.service
systemctl restart mce.service

%files
%defattr(-,root,root,-)
%{_datadir}/dbus-1/services/*.service
%config /etc/mce/*.ini
%config /etc/profiled/*.ini
%config %{_datadir}/ngfd/events.d/*.ini
%config %{_datadir}/lipstick/notificationcategories/*.conf
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/86x86/apps/%{name}.png
%{_datadir}/%{name}
/usr/lib/nemo-transferengine/plugins/*.so
/usr/lib/systemd/user/*.service
%attr(4755, root, root) %{_bindir}/*
