#!/bin/bash
set -e
echo 'pBuild 1.0'
echo 'Installing Pebble SDK and its Dependencies...'

cd ~ 

# Get the Pebble SDK and toolchain
PEBBLE_SDK_VER=${PEBBLE_SDK#PebbleSDK-}
if [ ! -d $HOME/pebble-dev/${PEBBLE_SDK} ]; then
  wget https://sdk.getpebble.com/download/${PEBBLE_SDK_VER} -O PebbleSDK.tar.gz
  wget http://assets.getpebble.com.s3-website-us-east-1.amazonaws.com/sdk/arm-cs-tools-ubuntu-universal.tar.gz
  # Build the Pebble directory
  mkdir ~/pebble-dev
  cd ~/pebble-dev
  # Extract the SDK
  tar -zxf ~/PebbleSDK.tar.gz
  # Extract the toolchain
  cd ~/pebble-dev/${PEBBLE_SDK}
  tar -zxf ~/arm-cs-tools-ubuntu-universal.tar.gz
fi

# Install the Python library dependencies locally
virtualenv --no-site-packages .env
source .env/bin/activate
pip install -r requirements.txt
deactivate
