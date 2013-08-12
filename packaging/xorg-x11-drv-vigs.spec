# >> macros
# << macros

Name:       xorg-x11-drv-vigs
Summary:    X.Org X server driver for VIGS
Version:    12.0.1
Release:    1
Group:      System/X Hardware Support
License:    Samsung
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  prelink
BuildRequires:  xorg-x11-xutils-dev
BuildRequires:  pkgconfig(xorg-server)
BuildRequires:  pkgconfig(randrproto)
BuildRequires:  pkgconfig(renderproto)
BuildRequires:  pkgconfig(fontsproto)
BuildRequires:  pkgconfig(xproto)
BuildRequires:  pkgconfig(videoproto)
BuildRequires:  pkgconfig(resourceproto)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(pixman-1)

%description
This package provides the driver for VIGS

%prep
%setup -q

# >> setup
# << setup

%build
# >> build pre
# << build pre

make %{?jobs:-j%jobs}

# >> build post
# << build post
%install
rm -rf %{buildroot}
# >> install pre
# << install pre
make INSTALL_DIR=%{buildroot}%{_libdir}/xorg/modules/drivers install

# >> install post
execstack -c %{buildroot}%{_libdir}/xorg/modules/drivers/vigs_drv.so
mkdir -p %{buildroot}/etc/X11/conf-i386-emulfb
cp packaging/display.conf %{buildroot}/etc/X11/conf-i386-emulfb
mkdir -p %{buildroot}/opt/home/app/.e/e/config/samsung
cp packaging/module.comp-tizen.cfg %{buildroot}/opt/home/app/.e/e/config/samsung
# << install post

%files
%defattr(-,root,root,-)
# >> files vigs
%{_libdir}/xorg/modules/drivers/*.so
/etc/X11/conf-i386-emulfb/display.conf
/opt/home/app/.e/e/config/samsung/module.comp-tizen.cfg
# << files vigs
