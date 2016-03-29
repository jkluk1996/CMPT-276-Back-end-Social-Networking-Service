#!/bin/bash
PROP=\"$3\"\:\"$4\"
curl -i -X put -H"$H" -d "{$PROP}" $B/UpdateEntityAdmin/DataTable/$1/$2
