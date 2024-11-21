#!/bin/sh

while [ 1 = 1 ]
do
    if [ ! $pid ]; then
        pid=`pidof $1`
    fi

    if [ ! $pid ]; then
        sleep 1s
        continue
    fi

    usleep 1000

    RssAnon=`cat /proc/$pid/status | grep RssAnon`
    VmRSS=`cat /proc/$pid/status | grep VmRSS`
	RSSFile=`cat /proc/$pid/status | grep RssFile`
    VmSize=`cat /proc/$pid/status | grep VmSize`
    MemAvailable=`cat /proc/meminfo | grep MemAvailable`
	MemFree=`cat /proc/meminfo | grep MemFree`
	MemCached=`cat /proc/meminfo | grep Cached`
	MemSlab=`cat /proc/meminfo | grep Slab`
	MemSUnreclaim=`cat /proc/meminfo | grep SUnreclaim`
    ret=$?
    if [ $ret -ne 0 ]; then
        unset pid
        continue
    fi

    RssAnonSize=$(echo ${RssAnon} | sed -r "s/RssAnon: *//g")
    RssAnonSize=${RssAnonSize%% kB*}

    if [ ! $tmp ]; then
      tmp=$RssAnonSize
    fi

    if [ $RssAnonSize -gt $tmp ]; then
      if [ $((RssAnonSize-tmp)) -gt 51200 ]; then
        echo "+++++++++++++"
        echo "Memory leak!!!"
        echo "-------------"
		exit
      fi
    fi
    Meminfo="$RssAnon, $RSSFile, $VmRSS, $VmSize, $MemAvailable, $MemFree, $MemCached, $MemSlab, $MemSUnreclaim"
    echo $Meminfo
    #echo $Meminfo >> pid_$1_rssanon.log
    sleep 1s
done

