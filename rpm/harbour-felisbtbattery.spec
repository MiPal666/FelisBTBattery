Name:       harbour-felisbtbattery
Summary:    Bluetooth audio battery monitor and connection control for Sailfish OS
Version:    0.2.1
Release:    1
Group:      Qt/Qt
License:    GPL-3.0-or-later AND GPL-2.0-or-later
URL:        https://github.com/MiPal666/FelisBTBattery
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   sailfish-components-bluetooth-qt5
Requires:   ofono >= 1.29
Provides:   felisbtbattery-ofono-plugin = 0.1.4
Obsoletes:  felisbtbattery-ofono-plugin < 0.1.5

BuildRequires:  make
BuildRequires:  gcc
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(ofono)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  desktop-file-utils

%description
Felis BT Battery Audio displays battery information actually reported by
Bluetooth devices through BlueZ and can connect or disconnect paired devices.
The RPM bundles an oFono companion plugin for common HFP audio battery reports.
It does not estimate charge or use vendor-specific battery protocols.

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5
%make_build
%make_build -C backend

%install
%qmake5_install
make -C backend DESTDIR=%{buildroot} install

desktop-file-install --delete-original --dir %{buildroot}%{_datadir}/applications %{buildroot}%{_datadir}/applications/*.desktop

%post
systemctl try-restart ofono.service >/dev/null 2>&1 || :
systemctl try-restart bluetooth.service >/dev/null 2>&1 || :

%postun
if [ "$1" -eq 0 ]; then
    systemctl try-restart ofono.service >/dev/null 2>&1 || :
    systemctl try-restart bluetooth.service >/dev/null 2>&1 || :
fi

%files
%defattr(-,root,root,-)
%license LICENSE
%license backend/LICENSE.GPL-2.0
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_libdir}/ofono/plugins/felisbtbattery.so
