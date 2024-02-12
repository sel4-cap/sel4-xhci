#!/bin/bash
export MICROKIT_BOARD="maaxboard"
export MICROKIT_CONFIG="debug"
export MICROKIT_TOOL="python -m microkit"

export MICROKIT_DIR="$HOME/xhci/microkit" #! change this to microkit directory
export MICROKIT_SDK="$MICROKIT_DIR/release/microkit-sdk-1.2.6"
export BOARD_DIR="$MICROKIT_SDK/board/$MICROKIT_BOARD/$MICROKIT_CONFIG"

export PYTHONPATH="$MICROKIT_DIR/tool"
export PATH="$MICROKIT_DIR/../compiler/bin:$PATH"

export SEL4_XHCI_PATH=$MICROKIT_DIR/example/maaxboard/xhci_stub
export BUILD_DIR=$SEL4_XHCI_PATH/xhci_build
export NETBSD_DIR="$SEL4_XHCI_PATH/netbsd/src"

# copy specs file 
cp $MICROKIT_DIR/../picolibc/picolib-microkit/picolibc.specs $SEL4_XHCI_PATH/libc/picolibc.specs

# make clean
mkdir -p $BUILD_DIR
if [ "$1" = "clean" ]; then
    make -C $SEL4_XHCI_PATH clean
elif [ "$1" = "rebuild" ]; then
    make -C $SEL4_XHCI_PATH clean
    make -C $SEL4_XHCI_PATH
else
    make -C $SEL4_XHCI_PATH
fi

# specific to capgemini implementation
echo 
TFTP_LOCATION="/var/lib/tftpboot/loader-josh.img"
sudo cp $BUILD_DIR/loader.img $TFTP_LOCATION
if [ $? -gt 0 ]; then
    echo "Error(s) detected, file not copied"
else
    echo Copied image file to "$TFTP_LOCATION".
fi