# >> macros
# << macros

Name:       xorg-x11-drv-vigs
Summary:    X.Org X server driver for VIGS
Version:    12.0.3
Release:    1
Group:      System/X Hardware Support
ExclusiveArch:  %{ix86}
License:    MIT
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  prelink
BuildRequires:  pkgconfig(xorg-macros)
BuildRequires:  pkgconfig(xorg-server)
BuildRequires:  pkgconfig(randrproto)
BuildRequires:  pkgconfig(renderproto)
BuildRequires:  pkgconfig(fontsproto)
BuildRequires:  pkgconfig(xproto)
BuildRequires:  pkgconfig(videoproto)
BuildRequires:  pkgconfig(resourceproto)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(pixman-1)

%description
This package provides the driver for VIGS

%prep
%setup -q


%build
rm -rf %{buildroot}

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp -af COPYING %{buildroot}/usr/share/license/%{name}
make INSTALL_DIR=%{buildroot}%{_libdir}/xorg/modules/drivers install

# >> install post
execstack -c %{buildroot}%{_libdir}/xorg/modules/drivers/vigs_drv.so
# << install post

%files
%defattr(-,root,root,-)
%{_libdir}/xorg/modules/drivers/*.so
/usr/share/license/%{name}
