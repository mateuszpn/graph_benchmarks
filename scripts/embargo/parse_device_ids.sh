#
# INTEL CONFIDENTIAL
#
# Copyright (C) 2022 Intel Corporation
#
# This software and the related documents are Intel copyrighted materials,
# and your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise,
# you may not use, modify, copy, publish, distribute, disclose or transmit this
# software or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express or
# implied warranties, other than those that are expressly stated in the License.

declare -A order=(
    [Bdw]=0
    [Bxt]=1
    [Cfl]=2
    [Glk]=3
    [Kbl]=4
    [Skl]=5
    [Ehl]=6
    [Icllp]=7
    [Lkf]=8
    [Adlp]=9
    [Adls]=10
    [Dg1]=11
    [Rkl]=12
    [Tgllp]=13
    [Xehp]=14
    [Dg2]=15
    [Pvc]=17
)

function add_device_index() (
    function get_index() (
        result=${order[$1]}
        if [ -z "$result" ]; then
            echo 99999
        else
            echo $result
        fi
    )

    while read line
    do
        line_as_array=($line)
        device_id=${line_as_array[0]}
        device_name=${line_as_array[1]}
        device_sorting_index=`get_index $device_name`

        echo "$device_sorting_index $device_name $device_id"
    done < "${1:-/dev/stdin}"
)

function remove_device_index() (
    sed -E "s/^[0-9]+ //g"
)

neo_path=$1
neo_devices_path="$neo_path/shared/source/dll/devices"
files_paths="$neo_devices_path/devices_base.inl $neo_devices_path/embargo/devices.inl"

cat $files_paths                                                        | # raw source code
    grep -E "(NAMED)?DEVICE"                                            | # filter only device definitions,        e.g. DEVICE(0x56A8, DG2_CONFIG, GTTYPE_GT4)
    sed -E "s/(NAMEDDEVICE|DEVICE)\(\s*([^,]+), ([^,]+)(,.*)?/\2 \3/g"  | # strip uninmportant parts of the macro, e.g. 0x56A8 DG2_CONFIG
    sed -E "s/_(HW_CONFIG|CONFIG|([0-9x]*))$//g"                        | # strip device type suffix,              e.g. 0x56A8 DG2
    sed -E 's/^([^ ]+) (.*)/\1 \L\u\2/g'                                | # convert device type to capitalized,    e.g. 0x56A8 Dg2
    add_device_index | sort -n -t' ' -k1  -s | remove_device_index      | # adds temporary index for sorting based on device name, sorts and removes the index
    sed -E "s/([^ ]+) (.*)/    INTEL_PRODUCT_ID\(\2, \1\)/g"              # wrap in ComputeBenchmarks macro,       e.g. INTEL_PRODUCT_ID(Dg2, 0x56A8)
