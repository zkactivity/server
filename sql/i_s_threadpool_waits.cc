/* Copyright(C) 2019 MariaDB

This program is free software; you can redistribute itand /or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111 - 1301 USA*/

#include <threadpool_generic.h>
#include <mysql_version.h>
#include <mysql/plugin.h>

#include <my_global.h>
#include <sql_class.h>
#include <table.h>
#include <mysql/plugin.h>
#include <sql_show.h>
#include <my_counter.h>


static ST_FIELD_INFO fields_info[] =
{
  {"REASON", 6, MYSQL_TYPE_STRING, 0, 0, 0, 0},
  {"COUNT",19,MYSQL_TYPE_LONGLONG,0,0, 0,0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};

/* See thd_wait_type enum for explanation*/
static const LEX_CSTRING wait_reasons[THD_WAIT_LAST]=
{
  {STRING_WITH_LEN("UNKNOWN")},
  {STRING_WITH_LEN("SLEEP")},
  {STRING_WITH_LEN("DISKIO")},
  {STRING_WITH_LEN("ROW_LOCK")},
  {STRING_WITH_LEN("GLOBAL_LOCK")},
  {STRING_WITH_LEN("META_DATA_LOCK")},
  {STRING_WITH_LEN("TABLE_LOCK")},
  {STRING_WITH_LEN("USER_LOCK")},
  {STRING_WITH_LEN("BINLOG")},
  {STRING_WITH_LEN("GROUP_COMMIT")},
  {STRING_WITH_LEN("SYNC")},
  {STRING_WITH_LEN("NET")}
};

extern Atomic_counter<unsigned long long> tp_waits[THD_WAIT_LAST];

static int fill_table(THD* thd, TABLE_LIST* tables, COND*)
{
  if (!all_groups)
    return 0;

  TABLE* table = tables->table;
  for (auto i=0; i < THD_WAIT_LAST; i++)
  {
    table->field[0]->store(wait_reasons[i].str, wait_reasons[i].length, system_charset_info);
    table->field[1]->store(tp_waits[i], true);
    if (schema_table_store_record(thd, table))
      return 1;
  }
  return 0;
}

static int reset_table()
{
  for (auto i = 0; i < THD_WAIT_LAST; i++)
   tp_waits[i]= 0;

  return 0;
}

static int init(void* p)
{
  ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*)p;
  schema->fields_info = fields_info;
  schema->fill_table = fill_table;
  schema->reset_table = reset_table;
  return 0;
}

static st_mysql_information_schema plugin_descriptor =
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

maria_declare_plugin(thread_pool_waits)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &plugin_descriptor,
  "THREAD_POOL_WAITS",
  "Vladislav Vaintroub",
  "Provides wait counters for threadpool.",
  PLUGIN_LICENSE_GPL,
  init,
  0,
  0x0100,
  NULL,
  NULL,
  "1.0",
  MariaDB_PLUGIN_MATURITY_STABLE
}
maria_declare_plugin_end;
