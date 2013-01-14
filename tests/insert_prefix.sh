#!/bin/sh
#
# Simple script for setting table prefixes in SQL-dump
if [ $# != 1 ]; then
	cat >&2 << EOH
	This is Drupal database prefixer.
	Usage:
	$0 original_db.sql >prefixed_db.sql
	- all tables will prefixed with 'prefix'
EOH
exit 1;
fi
          PRFX='${prefix}_';
	  sed "s/^DROP TABLE IF EXISTS \`/DROP TABLE IF EXISTS \`$PRFX/i;
	       s/^\/\*\!40000 ALTER TABLE \`/\/\*\!40000 ALTER TABLE \`$PRFX/i;
               s/^LOCK TABLES \`/LOCK TABLES \`$PRFX/i;
	       s/^CREATE TABLE \`/CREATE TABLE \`$PRFX/i;
               s/^CREATE TABLE IF NOT EXISTS \`/CREATE TABLE IF NOT EXISTS \`$PRFX/i;
	       s/^INSERT INTO \`/INSERT INTO \`$PRFX/i;
               s/^REPLACE \`/REPLACE \`$PRFX/i;
               s/^ALTER TABLE \`/ALTER TABLE \`$PRFX/i;
               s/InnoDB/MyISAM/i" $1
