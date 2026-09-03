#!/usr/bin/env bash
#
# gen_developer_key.sh
#
# Generates the universal personal developer signing keystore for Selfso Android apps
# and registers it into ~/.gradle/gradle.properties.
#
# Copyright (C) 2026, Charles Chiou
#

set -e

KEYSTORE_PATH="${HOME}/.android/selfso-release.jks"
KEY_ALIAS="selfso"
DNAME="CN=Charles Chiou, O=selfso, C=TW"
GRADLE_PROP_DIR="${HOME}/.gradle"
GRADLE_PROP="${GRADLE_PROP_DIR}/gradle.properties"

# Locate keytool
KEYTOOL="${HOME}/.jdks/jdk-21.0.4+7/bin/keytool"
if [ ! -x "$KEYTOOL" ]; then
    KEYTOOL="$(which keytool 2>/dev/null || true)"
fi

if [ -z "$KEYTOOL" ] || [ ! -x "$KEYTOOL" ]; then
    echo "Error: keytool not found." >&2
    exit 1
fi

PASSWORD="${1:-}"

if [ -f "$KEYSTORE_PATH" ]; then
    echo "Developer keystore already exists at: $KEYSTORE_PATH"
    if [ -z "$PASSWORD" ]; then
        if [ -f "$GRADLE_PROP" ] && grep -q "RELEASE_STORE_PASSWORD" "$GRADLE_PROP"; then
            echo "Existing configuration found in $GRADLE_PROP."
            exit 0
        fi
        echo "Error: Keystore exists but password was not provided and not found in $GRADLE_PROP." >&2
        echo "Usage: $0 <keystore-password>" >&2
        exit 1
    fi
else
    if [ -z "$PASSWORD" ]; then
        # Generate a strong random 24-character password
        PASSWORD="$(tr -dc 'A-Za-z0-9' </dev/urandom | head -c 24)"
    fi

    mkdir -p "${HOME}/.android"
    "$KEYTOOL" -genkeypair -v \
        -keystore "$KEYSTORE_PATH" \
        -alias "$KEY_ALIAS" \
        -keyalg RSA \
        -keysize 2048 \
        -validity 10000 \
        -storepass "$PASSWORD" \
        -keypass "$PASSWORD" \
        -dname "$DNAME"

    chmod 600 "$KEYSTORE_PATH"
    echo "Successfully generated developer keystore at: $KEYSTORE_PATH"
fi

# Configure ~/.gradle/gradle.properties
mkdir -p "$GRADLE_PROP_DIR"
touch "$GRADLE_PROP"
chmod 600 "$GRADLE_PROP"

# Remove any existing release signing entries to prevent duplicates
if grep -q "RELEASE_STORE_FILE" "$GRADLE_PROP"; then
    sed -i '/RELEASE_STORE_FILE/d' "$GRADLE_PROP"
    sed -i '/RELEASE_STORE_PASSWORD/d' "$GRADLE_PROP"
    sed -i '/RELEASE_KEY_ALIAS/d' "$GRADLE_PROP"
    sed -i '/RELEASE_KEY_PASSWORD/d' "$GRADLE_PROP"
fi

cat <<EOF >> "$GRADLE_PROP"

# Universal Selfso Android Signing Configuration
RELEASE_STORE_FILE=${KEYSTORE_PATH}
RELEASE_STORE_PASSWORD=${PASSWORD}
RELEASE_KEY_ALIAS=${KEY_ALIAS}
RELEASE_KEY_PASSWORD=${PASSWORD}
EOF

echo "Configured release credentials in $GRADLE_PROP"
echo "Certificate Identity: $DNAME"
echo "Key Alias: $KEY_ALIAS"
