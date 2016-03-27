#!/bin/bash
if [ $# -ne 1 ] ; then
	T=DataTable
else
	T=$1
fi
curl -i -X get $B/ReadEntityAdmin/$T