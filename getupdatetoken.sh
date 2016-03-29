#!/bin/sh
if [ $# -eq 2 ] ; then
  curl -X get -H 'Content-type: application/json' -d "{\"Password\": \"$2\"}" http://localhost:34570/GetUpdateToken/$1
else
  echo "usage: getupdatetoken userid password"
fi
