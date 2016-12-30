# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.27
# 

Name:       emumaster

# >> macros
# << macros

Summary:    An emulator for different gaming consoles etc.
Version:    0.3.1
Release:    1
Group:      Amusements/Games
License:    GPLv2
URL:        https://bitbucket.org/elemental/emumaster/wiki/Home
Source0:    %{name}_%{version}.tar.gz
Source100:  emumaster.yaml
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(qmfclient5)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5OpenGL)
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Sensors)
BuildRequires:  pkgconfig(Qt5SystemInfo)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  bluez-configs-mer
BuildRequires:  desktop-file-utils

%description
EmuMaster emulates popular consoles. It is written in Qt/QML. Item at Nokia Store store.ovi.com/content/207988 Donate version store.ovi.com/content/268920

%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
%qtc_qmake5
%qtc_make %{?_smp_mflags}
# << build pre



# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
%qmake5_install
# << install pre

# >> install post
# << install post

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_datadir}/applications/diskgallery.desktop
%{_datadir}/policy/etc/syspart.conf.d/*
/opt/%{name}
# >> files
# << files
