Name:       service-discovery-manager
Summary:    Service Discovery Manager for Distributed Web Engine
Version:    1.0.0
Release:    1
Group:      Development/Libraries
License:    Flora 1.1
URL:        https://github.com/Samsung/Castanets
Source0:    %{name}-%{version}.tar.gz

BuildRequires: pkgconfig(dbus-1)

%if "%{?tizen_profile_name}" == "tv"
%global TARGET_TYPE TIZEN_TV_PRODUCT
%else
%global TARGET_TYPE TIZEN_STANDARD_ARMV7L
%endif

%description
Service discovery manager server and client for distributed web engine

%prep
%setup -q

%build
# (TODO): enable parallel make
make \
    --directory=service_discovery_manager/Build \
    OS_TYPE="TIZEN" \
    TARGET_TYPE="%{TARGET_TYPE}" \
    #EOL

%install

mkdir -p %{buildroot}%{_bindir}

install -m 0755 service_discovery_manager/Build/BIN/%{TARGET_TYPE}/server_runner %{buildroot}%{_bindir}/server_runner
install -m 0755 service_discovery_manager/Build/BIN/%{TARGET_TYPE}/client_runner %{buildroot}%{_bindir}/client_runner
install -m 0644 service_discovery_manager/Build/BIN/%{TARGET_TYPE}/server.ini %{buildroot}%{_bindir}/server.ini
install -m 0644 service_discovery_manager/Build/BIN/%{TARGET_TYPE}/client.ini %{buildroot}%{_bindir}/client.ini

%files
%license service_discovery_manager/LICENSE.Flora
%attr(755,root,root) %{_bindir}/server_runner
%attr(755,root,root) %{_bindir}/client_runner
%attr(644,root,root) %{_bindir}/server.ini
%attr(644,root,root) %{_bindir}/client.ini

%changelog
