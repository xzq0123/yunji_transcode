#!/bin/bash

install_path=/tmp/ax_install
firmware_path=/lib/firmware/axcl
axcl_bin_path=/usr/bin/axcl
axcl_lib_path=/usr/lib/axcl
axcl_dat_path=/usr/bin/axcl/data
opt_bin_path=/opt/bin/axcl
opt_lib_path=/opt/lib
usr_lib_path=/usr/lib
usr_bin_path=/usr/bin

echo "removing directories and files..."
sudo rm -rf ${firmware_path}
sudo rm -rf ${axcl_bin_path}
sudo rm -rf ${axcl_lib_path}
sudo rm -rf ${axcl_dat_path}
sudo rm -rf ${install_path}
sudo rm -rf ${opt_bin_path}
sudo rm -rf ${opt_lib_path}/libaxcl_*
sudo rm -rf ${opt_lib_path}/ffmpeg
sudo rm -rf ${opt_lib_path}/libspdlog*
sudo rm -rf ${usr_bin_path}/axcl_*
sudo rm -rf ${usr_lib_path}/libaxcl_*

export LD_LIBRARY_PATH=

AXCL_LIB_BIN_PATH=$(grep -c '^export PATH=/usr/bin/axcl:$PATH$' /etc/profile)
if [ "${AXCL_LIB_BIN_PATH}" -ne 0 ]; then
    sudo sed -i '/^export PATH=\/usr\/bin\/axcl:\$PATH$/d' /etc/profile
fi

echo "removing library configuration and refreshing cache..."
sudo rm -f /etc/ld.so.conf.d/axcl.conf
sudo ldconfig
source /etc/profile

echo "cleanup completed."
