#!/bin/bash

lines=204
installpath=/tmp/ax_install
firmwarepath=/lib/firmware/axcl/
homepath=
axcl_bin_path=/usr/bin/axcl
axcl_lib_path=/usr/lib/axcl
axcl_dat_path=/usr/bin/axcl/data
OS_RELEASE_FILE="/etc/os-release"

ax_help()
{
    cat << EOH >&2
Usage: $0 [options]
    Options:
        --help | -h                     Print this message
        --install | -i                  Install axcl package
        --uninstall | -u                Uninstall axcl package
EOH
}

rebuild_drv()
{
    echo "[INFO] Rebuild driver..."
    cd ${installpath}/axcl/drv/pcie/driver
    make host=x86 clean all install >/dev/null 2>&1
    if [ $? -ne 0 ];then
        make host=x86 clean all install > ${homepath}/log/rebuild.log 2>&1
        echo "[ERROR] rebuild failed"
        cd - >/dev/null
        return 1
    fi
    cd - >/dev/null
    echo "[INFO] Rebuild driver finish."
    return 0
}

check_drv_vermagic()
{
    echo "[INFO] Check driver vermagic."
    pcie_host_ko=${installpath}/axcl/out/axcl_linux_x86/ko/ax_pcie_host_dev.ko
    if [ ! -f "$pcie_host_ko" ]; then
        # echo "[ERROR] no exist: $pcie_host_ko"
        rebuild_drv
        if [ $? -ne 0 ];then
            return 1
        fi
        return 0
    fi

    kernel_ver=$(uname -r)
    ko_vermagic=$(modinfo "$pcie_host_ko" | grep vermagic | awk '{print $2}')
    if [ "$kernel_ver"x != "$ko_vermagic"x ];then
        echo "[INFO]driver ko vermagic($ko_vermagic) is different from the os($kernel_ver), need rebuild driver"
        rebuild_drv
        if [ $? -ne 0 ];then
            return 1
        fi
    fi
    return 0
}

check_os_info()
{
    NAME=$(grep '^NAME=' "$OS_RELEASE_FILE" | cut -d= -f2 | tr -d '"')
    VERSION=$(grep '^VERSION=' "$OS_RELEASE_FILE" | cut -d= -f2 | tr -d '"')

    if [[ "$NAME" == *"CentOS Stream"* && "$VERSION" == *"9"* ]]; then
        cd ${installpath}/axcl/drv/pcie/driver
        patch -p3 < pcie_proc.patch >/dev/null 2>&1
    fi
}

install_pcie_drv()
{
    insmod ${installpath}/axcl/out/axcl_linux_x86/ko/ax_pcie_host_dev.ko
    insmod ${installpath}/axcl/out/axcl_linux_x86/ko/ax_pcie_msg.ko
    insmod ${installpath}/axcl/out/axcl_linux_x86/ko/ax_pcie_mmb.ko
    insmod ${installpath}/axcl/out/axcl_linux_x86/ko/axcl_host.ko
    chmod 666 /dev/msg_userdev
    chmod 666 /dev/ax_mmb_dev
    chmod 666 /dev/axcl_host
}

uninstall_pcie_drv()
{
    rmmod axcl_host
    rmmod ax_pcie_mmb
    rmmod ax_pcie_msg
    rmmod ax_pcie_host_dev
}

get_os_name() {
    local name=$(grep "^NAME=" /etc/os-release | cut -d'"' -f2)
    echo "$name" | awk '{print tolower($0)}'
}

ax_install()
{
    tail -n +$lines $0 >./axcl.tgz

    if [ ! -d "${installpath}" ];then
        mkdir -p ${installpath}
    fi
    tar zxvf ./axcl.tgz -C $installpath >/dev/null 2>&1
    homepath=`pwd`
    mkdir -p ${homepath}/log
    # insmod ${installpath}/axcl/out/axcl_linux_x86/ko/ax_pcie_host_dev.ko >${homepath}/log/load_drv_tmp_log 2>&1
    # cat ${homepath}/log/load_drv_tmp_log | grep -i "Invalid module format" >>/dev/null
    # if [ $? -eq 0 ];then
    #     rebuild_drv
    #     if [ $? -ne 0 ];then
    #         rm -rf ${homepath}/log/load_drv_tmp_log
    #         return 1
    #     fi
    # fi
    check_os_info
    check_drv_vermagic
    if [ $? -ne 0 ];then
        return 1
    fi

    echo "[INFO] axcl bin path: ${axcl_bin_path}"
    echo "[INFO] axcl lib path: ${axcl_lib_path}"

    mkdir -p ${axcl_bin_path}
    mkdir -p ${axcl_lib_path}
    mkdir -p ${axcl_dat_path}

    # cp sample && axcl.json
    cp -rf ${installpath}/axcl/out/axcl_linux_x86/bin/axcl_* ${axcl_bin_path}/
    chmod 777 ${axcl_bin_path}/*
    cp -rf ${installpath}/axcl/out/axcl_linux_x86/json/axcl.json ${axcl_bin_path}/
    chmod 666 ${axcl_bin_path}/axcl.json

    # cp ut
    cp -rf ${installpath}/axcl/out/axcl_linux_x86/bin/ut ${axcl_bin_path}/
    chmod 777 ${axcl_bin_path}/ut/*

    # cp data
    cp -rf ${installpath}/axcl/out/axcl_linux_x86/data/* ${axcl_dat_path}/

    # cp lib
    cp -rf ${installpath}/axcl/out/axcl_linux_x86/lib/libaxcl_* ${axcl_lib_path}/
    cp -rf ${installpath}/axcl/out/axcl_linux_x86/lib/ffmpeg ${axcl_lib_path}/
    cp -df ${installpath}/axcl/out/axcl_linux_x86/lib/libspdlog* ${axcl_lib_path}/

    # cp ax650_card.pac to /lib/firmware/axcl/
    if [ ! -d "${firmwarepath}" ];then
        mkdir -p ${firmwarepath}
    fi

    cp -rf ${installpath}/axcl/image/* ${firmwarepath}
    chmod 666 ${firmwarepath}/*

    # install pcie ko
    install_pcie_drv

    AXCL_LIB_BIN_PATH=$(grep -c 'export PATH=/usr/bin/axcl:$PATH' /etc/profile)
    if [ "${AXCL_LIB_BIN_PATH}" -eq 0 ]; then
        echo 'export PATH=/usr/bin/axcl:$PATH' | sudo tee -a /etc/profile > /dev/null
    fi

    rm -f /etc/ld.so.conf.d/axcl.conf
    echo '/usr/lib/axcl' | sudo tee /etc/ld.so.conf.d/axcl.conf > /dev/null
    sudo ldconfig

    echo "axcl install finish!"
}

ax_uninstall()
{
    uninstall_pcie_drv
    rm -rf ${axcl_dat_path}
    rm -rf ${axcl_bin_path}
    rm -rf ${axcl_lib_path}
    rm -rf /lib/firmware/axcl
    rm -f /etc/ld.so.conf.d/axcl.conf
    ldconfig
}

while [ -n "$*" ]
do
    case "$1" in
    -h | --help)
    ax_help
    exit 0
    ;;
    -i | --install)
    ax_install
    break
    ;;
    -u | --uninstall)
    ax_uninstall
    exit 0
    ;;
    esac
done

rm -rf ${installpath}
rm -rf axcl.tgz
exit 0
