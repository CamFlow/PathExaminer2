#!/bin/bash
cat <<EOF | sqlite3 db.sqlite
CREATE TABLE functions (Id INTEGER PRIMARY KEY, Name TEXT, File TEXT, Line INTEGER, Global INTEGER);
CREATE TABLE calls (Caller INTEGER, Callee INTEGER, PRIMARY KEY (Caller,Callee));
EOF
