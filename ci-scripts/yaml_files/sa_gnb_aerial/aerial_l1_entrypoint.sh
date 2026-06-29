#!/bin/bash
# SPDX-License-Identifier: MIT

#Clear logs
sudo rm -rf /tmp/phy.log
sudo rm -rf /tmp/logs/*

# Check if cuBB_SDK is defined, if not, use default path
cuBB_Path="${cuBB_SDK:-/opt/nvidia/cuBB}"

cd "$cuBB_Path" || exit 1

# Restart MPS
# Export variables
export CUDA_DEVICE_MAX_CONNECTIONS=8
export CUDA_MPS_PIPE_DIRECTORY=/var
export CUDA_MPS_LOG_DIRECTORY=/var

# Stop existing MPS
sudo -E echo quit | sudo -E nvidia-cuda-mps-control

# Start MPS
sudo -E nvidia-cuda-mps-control -d
sudo -E echo start_server -uid 0 | sudo -E nvidia-cuda-mps-control

# Start cuphycontroller_scf
# Check if an argument is provided
if [ $# -eq 0 ]; then
# No argument provided, use default value
               serverVendorAndModel=$(cat /sys/devices/virtual/dmi/id/board_vendor)
               serverVendorAndModel+="-"
               serverVendorAndModel+=$(cat /sys/devices/virtual/dmi/id/board_name)
               echo $serverVendorAndModel
               case $serverVendorAndModel in
       "Dell Inc.-06V45N")
               argument="P5G_FXN_R750"
               ;;
       "GIGABYTE-MU71-SU0-00")
#              argument="P5G_FXN"
              argument="P5G_BENETEL"
#               argument="P5G_LITEON"
               ;;
       "Supermicro-G1SMH-G")
               argument="P5G_WNC_GH"
               #argument="P5G_FXN_GH"
               ;;
       *)
               echo "Unrecognized server: $serverVendorAndModel"
               exit
               ;;
       esac
else
    # Argument provided, use it
    argument="$1"
fi
configFile=${cuBB_SDK}/cuPHY-CP/cuphycontroller/config/cuphycontroller_${argument}.yaml
# Read l1_config.yaml for parameters to set in the config file used by cuphycontroller
# Define the source file path
SOURCE_FILE="l1_config.yaml"

# Construct the target file path dynamically
TARGET_FILE_NAME="cuphycontroller_${argument}.yaml"
TARGET_FILE="${configFile}"
# Check if the target file exists before performing updates
if [ ! -f "$TARGET_FILE" ]; then
    echo "Target file $TARGET_FILE does not exist. Exiting..."
    exit 1
fi


# Check if the source YAML file exists; skip YAML update if not found
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Source file $SOURCE_FILE does not exist. Skipping YAML update..."
else
    # Extract all parameter names and values from the source YAML file, ignoring commented lines
    PARAMS=$(grep -v "^[[:space:]]*#" "$SOURCE_FILE" | grep -E "^[[:space:]]*[a-zA-Z0-9_]+:" | awk -F ':' '{print $1}' | tr -d ' ')
    # Loop through each detected parameter and update it in the target YAML file
    for param in $PARAMS; do
        echo "Processing $param"
        # Read the value of the current parameter from the source file, ignoring commented and blank lines
        value=$(grep "^[[:space:]]*${param}:" "$SOURCE_FILE" | grep -v "^[[:space:]]*#" | awk '{print $2}')
        # Perform sed replacement if the value is not empty
        if [ -n "$value" ]; then
            # Use sed to update the target YAML file with the value from the source
            sed -i "s|$param:.*|$param: $value|" "$TARGET_FILE"
            echo "Updated $param in $TARGET_FILE_NAME with value: $value"
        else
            echo "Parameter $param found in $SOURCE_FILE but has an empty value, skipping..."
        fi
    done
    echo "YAML update completed."
fi

# Uncomment for below config. Because aerial is mounted as a volume the $configFile needs to be reverted.
#config="UL-Heavy" # Commenting this line won't revert the change
if [[ $config = "UL-Heavy" ]]; then
       echo "Doing UL-heavy"
       sudo -E sed -i "s/shm_log_level: 4/shm_log_level: 5/" ${configFile}
       sudo -E sed -i "s/pusch_aggr_per_ctx:.*/pusch_aggr_per_ctx: 12/" ${configFile}
       sudo -E sed -i "s/prach_aggr_per_ctx.*/prach_aggr_per_ctx: 4/" ${configFile}
       sudo -E sed -i "s/ul_input_buffer_per_cell:.*/ul_input_buffer_per_cell: 20/" ${configFile}
fi
# do the same process for nvlog_config.yaml /opt/nvidia/cuBB/cuPHY/nvlog/config/nvlog_config.yaml
# Define the source file path
SOURCE_FILE="nvlog_config.yaml"

# Construct the target file path dynamically
TARGET_FILE_NAME="nvlog_config.yaml"
TARGET_FILE="/opt/nvidia/cuBB/cuPHY/nvlog/config/${TARGET_FILE_NAME}"
# Check if the target file exists before performing updates
if [ ! -f "$TARGET_FILE" ]; then
    echo "Target file $TARGET_FILE does not exist. Exiting..."
    exit 1
fi


# Check if the source YAML file exists; skip YAML update if not found
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Source file $SOURCE_FILE does not exist. Skipping YAML update..."
else
    # Extract all parameter names and values from the source YAML file, ignoring commented lines
    PARAMS=$(grep -v "^[[:space:]]*#" "$SOURCE_FILE" | grep -E "^[[:space:]]*[a-zA-Z0-9_]+:" | awk -F ':' '{print $1}' | tr -d ' ')
    # Loop through each detected parameter and update it in the target YAML file
    for param in $PARAMS; do
        echo "Processing $param"
        # Read the value of the current parameter from the source file, ignoring commented and blank lines
        value=$(grep "^[[:space:]]*${param}:" "$SOURCE_FILE" | grep -v "^[[:space:]]*#" | awk '{print $2}')
        # Perform sed replacement if the value is not empty
        if [ -n "$value" ]; then
            # Use sed to update the target YAML file with the value from the source
            sed -i "s|$param:.*|$param: $value|" "$TARGET_FILE"
            echo "Updated $param in $TARGET_FILE_NAME with value: $value"
        else
            echo "Parameter $param found in $SOURCE_FILE but has an empty value, skipping..."
        fi
    done
    echo "YAML update completed."
fi

export AERIAL_LOG_PATH=/var/log/aerial

trap "sudo -E ./build.$(uname -m)/cuPHY-CP/gt_common_libs/nvIPC/tests/pcap/pcap_collect nvipc /tmp ; sudo -E mv /tmp/nvipc*.pcap $AERIAL_LOG_PATH ; sudo -E mv /tmp/phy.log $AERIAL_LOG_PATH ; exit " SIGTERM SIGKILL

sudo -E cat  "$cuBB_Path"/cuPHY-CP/cuphycontroller/config/cuphycontroller_$argument.yaml

sudo -E "$cuBB_Path"/build.$(uname -m)/cuPHY-CP/cuphycontroller/examples/cuphycontroller_scf $argument &


while true; do
  sleep 1
done
