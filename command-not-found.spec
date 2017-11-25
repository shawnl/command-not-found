%global _vpath_srcdir sdk/%{name}/projects/meson

Name:           command-not-found
Version:        0.4
Release:        1%{?dist}
Summary:        Suggest installation of packages in interactive bash sessions

License:        LGPL-2.1+
URL:            https://github.com/shawnl/command-not-found
Source:         %{url}sdk/files/%{name}_%{version}.zip

BuildRequires:  meson
BuildRequires:  gcc

%package command-not-found-data
Summary:        Data for %{s}
Requires:       %{name}%{?_isa} > %{?epoch:%{epoch}:}%{version}-%{release}

%description devel
%{summary}.

%prep
%autosetup -c

%build
%meson
%meson_build

%install
%meson_install

%check
%meson_test

%files
%{_libdir}/command-not-found
