Name: axcl_host
Version: %{?SDK_VERSION:%{SDK_VERSION}}%{!?SDK_VERSION:1.0}
Release: %{?SDK_RELEASE:%{SDK_RELEASE}}%{!?SDK_RELEASE:1}%{?dist}
Summary: The "axcl_host" package is used for multimedia data exchange between the host and AX PCIe devices.

%define _lib /lib
%define _libdir %{_exec_prefix}%{_lib}
%define _kver %(uname -r)

License: GPL
Packager: root <root@localhost>
Source0: %{_topdir}/SOURCE/axcl.tar.gz

Requires(post): make
Requires(post): gcc
Requires: gcc >= 9.4.0

%description
This package contains PCIe driver, samples, binaries, and libraries.

%prep
%setup -n axcl -q

%install
mkdir -p %{buildroot}%{_bindir}/axcl
mkdir -p %{buildroot}%{_libdir}/axcl
mkdir -p %{buildroot}%{_usrsrc}/axcl
mkdir -p %{buildroot}%{_includedir}/axcl
mkdir -p %{buildroot}%{_sysconfdir}/ld.so.conf.d
mkdir -p %{buildroot}%{_sysconfdir}/modules-load.d
mkdir -p %{buildroot}%{_sysconfdir}/udev/rules.d
mkdir -p %{buildroot}%{_sysconfdir}/profile.d
mkdir -p %{buildroot}%{_lib}/firmware/axcl
mkdir -p %{buildroot}%{_lib}/modules/$(uname -r)/extra
cp -rf %{_builddir}/axcl/image/ax650_card.pac %{buildroot}%{_lib}/firmware/axcl
cp -rf %{_builddir}/axcl/axcl.conf %{buildroot}%{_sysconfdir}/ld.so.conf.d
cp -rf %{_builddir}/axcl/axcl_pcie.conf %{buildroot}%{_sysconfdir}/modules-load.d
cp -rf %{_builddir}/axcl/axcl_host.rules %{buildroot}%{_sysconfdir}/udev/rules.d
cp -rf %{_builddir}/axcl/axcl.sh %{buildroot}%{_sysconfdir}/profile.d
cp -rf %{_builddir}/axcl/lib/* %{buildroot}%{_libdir}/axcl
cp -rf %{_builddir}/axcl/bin/* %{buildroot}%{_bindir}/axcl
cp -rf %{_builddir}/axcl/json/* %{buildroot}%{_bindir}/axcl
cp -rf %{_builddir}/axcl/data %{buildroot}%{_bindir}/axcl
cp -rf %{_builddir}/axcl/include/* %{buildroot}%{_includedir}/axcl
cp -rf %{_builddir}/axcl/build %{buildroot}%{_usrsrc}/axcl
cp -rf %{_builddir}/axcl/drv %{buildroot}%{_usrsrc}/axcl

%post
cd %{_usrsrc}/axcl/drv/pcie/driver
os_version=$(cat /etc/os-release | grep '^VERSION_ID=' | cut -d '=' -f2 | tr -d '"')
os_name=$(cat /etc/os-release | grep '^NAME=' | cut -d '=' -f2 | tr -d '"')
if [[ ${os_name} == "CentOS Stream" && ${os_version} == "9" ]]; then
    patch -p3 < pcie_proc.patch >/dev/null 2>&1
fi
make host=x86 clean all install -j8 >/dev/null 2>&1
mkdir -p %{_lib}/modules/$(uname -r)/extra
cp -rf %{_usrsrc}/axcl/out/axcl_linux_x86/ko/* %{_lib}/modules/$(uname -r)/extra
depmod -a
ldconfig
modprobe ax_pcie_host_dev
modprobe ax_pcie_msg
modprobe ax_pcie_mmb
modprobe axcl_host

%files
%{_bindir}/axcl
%{_libdir}/axcl
%{_usrsrc}/axcl
%{_includedir}/axcl
%{_sysconfdir}/ld.so.conf.d/axcl.conf
%{_sysconfdir}/modules-load.d/axcl_pcie.conf
%{_sysconfdir}/udev/rules.d/axcl_host.rules
%{_sysconfdir}/profile.d/axcl.sh
%{_lib}/firmware/axcl

%preun
modprobe -r axcl_host
modprobe -r ax_pcie_mmb
modprobe -r ax_pcie_msg
modprobe -r ax_pcie_host_dev
rm -rf %{_lib}/modules/$(uname -r)/extra
rm -rf %{_usrsrc}/axcl/build/out
rm -rf %{_usrsrc}/axcl/out

%postun
depmod -a
ldconfig

%changelog
* Thu Oct 30 2024 root <root@localhost> - 1.0-1
- Update to 1.0-1