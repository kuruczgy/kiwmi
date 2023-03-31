#!/bin/sh
# Based on https://stackoverflow.com/a/34672970

export WLR_WL_OUTPUTS=1
export KIWMI_EMBED=1

CMD='./build/kiwmi/kiwmi'
BUILD_CMD='ninja -C build'

# swaymsg 'for_window [title="wlroots - .*"] move to workspace 4'

sigint_handler()
{
  rkill $PID
  exit
}

trap sigint_handler SIGINT

$BUILD_CMD

while true; do
  clear
  $CMD &
  PID=$!
  inotifywait -e close_write -r ./contrib
  rkill $PID
  $BUILD_CMD
done

