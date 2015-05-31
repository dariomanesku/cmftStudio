#!/bin/sh
#
# Copyright 2015 Dario Manesku. All rights reserved.
# License: http://www.opensource.org/licenses/BSD-2-Clause
#

# Use as: .makebg.sh VIMSERVERNAME MAKEPRG LOGFILE TEMPFILE

server="${1:-VIM}"
makeprg="${2:-make}"
logfile="${3:-make.log}"
tempfile="${4:-.make.tmp}"

{
  echo -n > $logfile
  echo '-----------------------------------------' >> "$tempfile"
  date >> "$tempfile"
  echo '-----------------------------------------' >> "$tempfile"

  # sed removes some words to prevent vim from misparsing a filename
  exec 3<> $tempfile
  $makeprg >&3 2>&1
  success=$?
  exec 3>&1
  sed -i 's/In file included from //' $tempfile

  cat "$logfile" >> "$tempfile"
  mv "$tempfile" "$logfile";
  vim --servername "$server" --remote-send "<Esc>:cgetfile $logfile<CR>" ;

  if [ $success -eq 0 ]; then
      vim --servername "$server" --remote-send "<Esc>:redraw | :echo \"Build successful.\"<CR>" ;
  else
      vim --servername "$server" --remote-send "<Esc>:redraw | :echo \"Build ERROR!\"<CR>" ;
  fi

} &
