systemd_dep = dependency('systemd')

systemd_system_unit_dir = systemd_dep.get_pkgconfig_variable('systemdsystemunitdir')

install_data('agl-service-hvac.service', install_dir : systemd_system_unit_dir)
