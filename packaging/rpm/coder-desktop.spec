%global appname coder-desktop

Name:           %{appname}
Version:        0.1.0
Release:        1%{?dist}
Summary:        Coder Desktop for Linux — Remote development workspace manager

License:        AGPL-3.0-only
URL:            https://coder.com
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  golang >= 1.21
BuildRequires:  pkgconfig
BuildRequires:  qt6-qtbase-devel >= 6.5
BuildRequires:  qt6-qtdeclarative-devel >= 6.5
BuildRequires:  qt6-qtquickcontrols2-devel >= 6.5
BuildRequires:  libsecret-devel
BuildRequires:  wlroots-devel >= 0.19
BuildRequires:  wayland-devel
BuildRequires:  wayland-protocols-devel
BuildRequires:  libGL-devel

Requires:       qt6-qtbase >= 6.5
Requires:       qt6-qtdeclarative >= 6.5
Requires:       qt6-qtquickcontrols2 >= 6.5
Requires:       libsecret
Recommends:     wlroots >= 0.19

%description
Coder Desktop provides a native Linux desktop experience for managing
Coder remote development workspaces. Features include a built-in VPN
tunnel, system tray integration, workspace lifecycle management, a Data
Loss Prevention compositor, and native credential storage via libsecret.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_bindir}/%{appname}
%dir %{_libdir}/%{appname}
%{_libdir}/%{appname}/libcoderdlp.so
%{_libdir}/%{appname}/libcodervpn.so
%{_libdir}/%{appname}/mutagen
%{_libdir}/%{appname}/mutagen-agents.tar.gz
%{_datadir}/applications/com.coder.CoderDesktop.desktop
%{_datadir}/icons/hicolor/scalable/apps/coder-desktop.svg

%changelog
* Mon Jan 01 2025 Coder Technologies Inc. <support@coder.com> - 0.1.0-1
- Initial package
