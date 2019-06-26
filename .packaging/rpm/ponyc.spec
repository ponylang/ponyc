%global ponyc_version %(ls %{_sourcedir} | egrep -o '[0-9]+\.[0-9]+\.[0-9]+' || cat ../../VERSION)
%global release_version 1

%ifarch x86_64
%global arch_build_args arch=x86-64 tune=intel
%endif

%if 0%{?el7}
%global arch_build_args arch=x86-64 tune=generic
%global extra_build_args LLVM_CONFIG=/usr/lib64/llvm3.9/bin/llvm-config
%else
%if %{?_vendor} == suse
%global extra_build_args default_ssl='openssl_1.1.x'
%else
%global extra_build_args default_ssl='openssl_1.1.x' LLVM_CONFIG=/usr/lib64/llvm3.9/bin/llvm-config
%endif
%endif

Name:       ponyc
Version:    %{ponyc_version}
Release:    %{release_version}%{?dist}
Packager:   Pony Core Team <buildbot@ponylang.io>
Summary:    Compiler for the pony programming language.
# For a breakdown of the licensing, see PACKAGE-LICENSING
License:    BSD
URL:        https://github.com/ponylang/ponyc
Source0:    https://github.com/ponylang/ponyc/archive/%{version}.tar.gz
BuildRequires:  git
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  pcre2-devel
BuildRequires:  zlib-devel
BuildRequires:  ncurses-devel

%if %{?_vendor} == suse
BuildRequires:  libopenssl-devel
BuildRequires:  binutils-gold
BuildRequires:  llvm-devel
%else
BuildRequires:  openssl-devel
BuildRequires:  libatomic
BuildRequires:  llvm3.9-devel
%endif

%if 0%{?el7}
BuildRequires:  llvm3.9-static
%endif

Requires:  gcc-c++
Requires:  openssl-devel
Requires:  pcre2-devel

%if %{?_vendor} == suse
Requires:  binutils-gold
%endif

%description
Compiler for the pony programming language.

Pony is an open-source, actor-model, capabilities-secure, high performance programming language
http://www.ponylang.io

%global debug_package %{nil}

%prep
%setup

%build
%{?build_command_prefix}make %{?arch_build_args} %{?extra_build_args} prefix=/usr %{?_smp_mflags} test-ci%{?build_command_postfix}

%install
%{?build_command_prefix}make install %{?arch_build_args} %{?extra_build_args} prefix=%{_prefix} DESTDIR=$RPM_BUILD_ROOT%{?build_command_postfix}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_prefix}/bin/ponyc
%{_prefix}/lib/libponyrt-pic.a
%{_prefix}/lib/libponyc.a
%{_prefix}/lib/pony
%{_prefix}/lib/libponyrt.a
%{_prefix}/include/pony.h
%{_prefix}/include/pony

%changelog
* Tue May 29 2018 Dipin Hora <dipin@wallaroolabs.com> 0.22.2-1
- Initial version of spec file
