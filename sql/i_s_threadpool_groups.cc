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

#include <mysql_version.h>
#include <mysql/plugin.h>

#include <my_global.h>
#include <sql_class.h>
#include <table.h>
#include <mysql/plugin.h>
#include <sql_show.h>
#include <threadpool_generic.h>

static ST_FIELD_INFO fields_info[] =
{
  {"GROUP_ID", 6, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"CONNECTIONS", 19, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"THREADS", 19, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"ACTIVE_THREADS",19, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"STANDBY_THREADS", 19, MYSQL_TYPE_LONGLONG, 0, 0, 0,0},
  {"QUEUE_LENGTH", 19, MYSQL_TYPE_LONGLONG, 0,0, 0, 0},
  {"HAS_LISTENER",1,MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"IS_STALLED",1,MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};

static int fill_table(THD* thd, TABLE_LIST* tables, COND*)
{
  if (!all_groups)
    return 0;

  TABLE* table = tables->table;
  for (uint i = 0; i < threadpool_max_size && all_groups[i].pollfd != INVALID_HANDLE_VALUE; i++)
  {
    thread_group_t* group = &all_groups[i];
    /* ID */
    table->field[0]->store(i, true);
    /* CONNECTION_COUNT */
    table->field[1]->store(group->connection_count, true);
    /* THREAD_COUNT */
    table->field[2]->store(group->thread_count, true);
    /* ACTIVE_THREAD_COUNT */
    table->field[3]->store(group->active_thread_count, true);
    /* STANDBY_THREAD_COUNT */
    table->field[4]->store(group->waiting_threads.elements(), true);
    /* QUEUE LENGTH */
    uint queue_len = group->queues[TP_PRIORITY_LOW].elements()
      + group->queues[TP_PRIORITY_HIGH].elements();
    table->field[5]->store(queue_len, true);
    /* HAS_LISTENER */
    table->field[6]->store((longlong)(group->listener != 0), true);
    /* IS_STALLED */
    table->field[7]->store(group->stalled, true);

    if (schema_table_store_record(thd, table))
      return 1;
  }
  return 0;
}


static int init(void* p)
{
  ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*)p;
  schema->fields_info = fields_info;
  schema->fill_table = fill_table;
  return 0;
}

static struct st_mysql_information_schema plugin_descriptor =
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

maria_declare_plugin(thread_pool_groups)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &plugin_descriptor,
  "THREAD_POOL_GROUPS",
  "Vladislav Vaintroub",
  "Provides information about threadpool groups.",
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
