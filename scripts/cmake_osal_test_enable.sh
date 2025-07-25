# SPDX-FileCopyrightText: 2017-2021 Sony Semiconductor Solutions Corporation
#
# SPDX-License-Identifier: Apache-2.0

################################################################################
#!/bin/sh

cd `dirname $0`

# Execute common processing script
. ./common.sh

echo "Set the following option in CMake ->"
echo "- SENSCORD_TEST_OSAL=ON"
echo "- CMAKE_BUILD_TYPE=Debug"
echo ""

cmake ../ -DSENSCORD_TEST_OSAL=ON -DCMAKE_BUILD_TYPE=Debug
