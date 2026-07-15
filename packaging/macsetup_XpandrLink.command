#!/bin/sh
# Remove quarantine from this script first
xattr -d com.apple.quarantine "$0" >/dev/null 2>&1 || true
CURRENT_LOCATION=$(dirname "$0")
cd "$CURRENT_LOCATION"
echo
echo "Clearing extended attributes for XpandrLink"
echo "Location: $CURRENT_LOCATION"
echo
find "$CURRENT_LOCATION" -name 'XpandrLink*' \
    -exec echo "Clearing attributes for: {}" \; \
    -exec xattr -cr {} \; \
     2>&2
echo
echo "Done. XpandrLink.app will now open normally, and any DAW should load"
echo "the AU/VST3 without a Gatekeeper block."
