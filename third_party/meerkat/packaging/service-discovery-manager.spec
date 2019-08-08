Name:       service-discovery-manager
Summary:    Service Discovery Manager for Distributed Web Engine
Version:    1.0.0
Release:    1
Group:      Development/Libraries
License:    Flora 1.1
URL:        https://github.com/Samsung/Castanets
Source0:    %{name}-%{version}.tar.gz

BuildRequires: pkgconfig(dbus-1)

%global BASE_FOLDER third_party/meerkat
%global BUILD_FOLDER %{BASE_FOLDER}/Build
%global OUT_FOLDER %{BUILD_FOLDER}/BIN

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
    --directory=%{BUILD_FOLDER} \
    OS_TYPE="TIZEN" \
    TARGET_TYPE="%{TARGET_TYPE}" \
    #EOL

%install

mkdir -p %{buildroot}%{_bindir}

install -m 0755 %{OUT_FOLDER}/%{TARGET_TYPE}/server_runner %{buildroot}%{_bindir}/server_runner
install -m 0755 %{OUT_FOLDER}/%{TARGET_TYPE}/client_runner %{buildroot}%{_bindir}/client_runner
install -m 0644 %{OUT_FOLDER}/%{TARGET_TYPE}/server.ini %{buildroot}%{_bindir}/server.ini
install -m 0644 %{OUT_FOLDER}/%{TARGET_TYPE}/client.ini %{buildroot}%{_bindir}/client.ini

%files
%license %{BASE_FOLDER}/LICENSE.Flora
%attr(755,root,root) %{_bindir}/server_runner
%attr(755,root,root) %{_bindir}/client_runner
%attr(644,root,root) %{_bindir}/server.ini
%attr(644,root,root) %{_bindir}/client.ini

%changelog
