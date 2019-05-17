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

static ST_FIELD_INFO fields_info[] =
{
  {"GROUP_ID", 6, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"THREAD_CREATIONS",19,MYSQL_TYPE_LONGLONG,0,0, 0,0},
  {"THREAD_CREATIONS_DUE_TO_STALL",19,MYSQL_TYPE_LONGLONG,0,0, 0,0},
  {"WAKES",19,MYSQL_TYPE_LONGLONG,0,0, 0,0},
  {"WAKES_DUE_TO_STALL",19,MYSQL_TYPE_LONGLONG,0,0, 0,0},
  {"THROTTLES",19,MYSQL_TYPE_LONGLONG,0,0, 0,0},
  {"STALLS",19,MYSQL_TYPE_LONGLONG,0,0, 0, 0},
  {"POLLS_BY_LISTENER",19,MYSQL_TYPE_LONGLONG,0,0, 0, 0},
  {"POLLS_BY_WORKER",19,MYSQL_TYPE_LONGLONG,0,0, 0, 0},
  {"DEQUEUES_BY_LISTENER",19,MYSQL_TYPE_LONGLONG,0,0, 0, 0},
  {"DEQUEUES_BY_WORKER",19,MYSQL_TYPE_LONGLONG,0,0, 0, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};

static int fill_table(THD* thd, TABLE_LIST* tables, COND*)
{
#ifndef EMBEDDED_LIBRARY
  if (!all_groups)
    return 0;

  TABLE* table = tables->table;
  for (uint i = 0; i < threadpool_max_size && all_groups[i].pollfd != INVALID_HANDLE_VALUE; i++)
  {
    table->field[0]->store(i, true);
    thread_group_t* group = &all_groups[i];

    mysql_mutex_lock(&group->mutex);
    thread_group_counters_t* counters = &group->counters;
    table->field[1]->store(counters->thread_creations, true);
    table->field[2]->store(counters->thread_creations_due_to_stall, true);
    table->field[3]->store(counters->wakes, true);
    table->field[4]->store(counters->wakes_due_to_stall, true);
    table->field[5]->store(counters->throttles, true);
    table->field[6]->store(counters->stalls, true);
    table->field[7]->store(counters->polls_by_listener,true);
    table->field[8]->store(counters->polls_by_worker, true);
    table->field[9]->store(counters->dequeues_by_listener, true);
    table->field[10]->store(counters->dequeues_by_worker, true);
    mysql_mutex_unlock(&group->mutex);
    if (schema_table_store_record(thd, table))
      return 1;
  }
  return 0;
#else
  return 0;
#endif /*EMBEDDED_LIBRARY*/
}

static int reset_table()
{
#ifndef EMBEDDED_LIBRARY
  for (uint i = 0; i < threadpool_max_size && all_groups[i].pollfd != INVALID_HANDLE_VALUE; i++)
  {
    thread_group_t* group = &all_groups[i];
    mysql_mutex_lock(&group->mutex);
    memset(&group->counters, 0, sizeof(group->counters));
    mysql_mutex_unlock(&group->mutex);
  }
  return 0;
#else
  return 0;
#endif /*EMBEDDED_LIBRARY*/
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

maria_declare_plugin(thread_pool_stats)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &plugin_descriptor,
  "THREAD_POOL_STATS",
  "Vladislav Vaintroub",
  "Provides performance counter information for threadpool.",
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
