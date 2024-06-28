#!/bin/bash
if [ -z "$LOCAL_RUN" ]; then
    echo "LOCAL_RUN is not set, run: source setup.sh"
    exit 1
fi

# Function to set a configuration variable in a specified file
set_conf() {
  local file_name="$1"  # First argument is the file name
  local var="$2"        # Second argument is the variable name
  local value="$3"      # Third argument is the value to set

  # Check if the file exists
  config_file="$MUCHI_ROOT/src/configs/$file_name.h"
  if [ ! -f "$config_file" ]; then
    echo "File $config_file does not exist"
    exit 1
  fi


  # Use sed to update the file, with platform-specific handling
  if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    sed -i '' -E "s/([[:alnum:]_]+[[:space:]]+${var}[[:space:]]*=[[:space:]]*)[^;]*/\1${value}/" "$config_file"
  else
    # Linux and other Unix-like systems
    sed -i -E "s/([[:alnum:]_]+[[:space:]]+${var}[[:space:]]*=[[:space:]]*)[^;]*/\1${value}/" "$config_file"
  fi
}
