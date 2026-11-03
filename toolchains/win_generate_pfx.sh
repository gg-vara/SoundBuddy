#! /bin/bash

# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Get the directory of the script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Get the current working directory
CURRENT_DIR="$(pwd)"

# Check if the script is run from its own directory
if [ "$SCRIPT_DIR" != "$CURRENT_DIR" ]; then
    echo "Please run this script from its own directory (cd to the .sh script before executing)."
    exit 1
fi

# stops if any command fails
set -e

# create recipient folder if it do not exist
mkdir -p ../certs

KEY_PATH="../certs/key.pem"
CERT_PATH="../certs/cert.crt"
PFX_PATH="../certs/all.pfx"

#
# generate both certificates and private key
#

#
if [[ ! -f "$KEY_PATH" || ! -f "$CERT_PATH" ]]; then
    read -p ">>>> WARNING !!! Please make sure that publisher that you will input is the same as the one defined as CMakeLists.txt APP_PUBLISHER_ORG ! Press any key to continue."
    echo ">> Creating new certificate && private key..."
    openssl genrsa -out $KEY_PATH 2048
    openssl req -x509 -new -nodes -key $KEY_PATH -sha256 -days 10000 -out $CERT_PATH
fi

# generate .pfx without empty password
if [[ ! -f "$PFX_PATH" ]]; then
    openssl pkcs12 -export -out $PFX_PATH -inkey $KEY_PATH -in $CERT_PATH -passout pass:
fi


echo ">> Done !"