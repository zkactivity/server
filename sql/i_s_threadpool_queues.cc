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

static ST_FIELD_INFO field_info[] =
{
  {"GROUP_ID", 6, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"POSITION",6,MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"PRIORITY", 1, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"CONNECTION_ID", 19, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"QUEUEING_TIME_MICROSECONDS", 19, MYSQL_TYPE_LONGLONG, 0,0,0},
  {0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

typedef connection_queue_t::Iterator connection_queue_iterator;

static int fill_table(THD* thd, TABLE_LIST* tables, COND*)
{
  if (!all_groups)
    return 0;

  TABLE* table = tables->table;
  for (uint group_id = 0; 
       group_id < threadpool_max_size && all_groups[group_id].pollfd != INVALID_HANDLE_VALUE; 
       group_id++)
  {
    thread_group_t* group = &all_groups[group_id];

    mysql_mutex_lock(&group->mutex);
    bool err=false;
    int pos= 0;
    ulonglong now = microsecond_interval_timer();
    for(uint prio=0; prio < NQUEUES && !err; prio++)
    {
      connection_queue_iterator it(group->queues[prio]);
      TP_connection_generic* c;
      while((c= it++) != 0)
      { 
         /* GROUP_ID */
         table->field[0]->store(group_id, true);
         /* POSITION */
         table->field[1]->store(pos++, true);
         /* PRIORITY */
         table->field[2]->store(prio, true);
         /* CONNECTION_ID */
         table->field[3]->store(c->thd->thread_id, true);
         /* QUEUEING_TIME */
         table->field[4]->store(now - c->enqueue_time, true);

         err = schema_table_store_record(thd, table);
         if (err)
           break;
      }
    }
    mysql_mutex_unlock(&group->mutex);
    if(err)
      return 1;
  }
  return 0;
}

static int init(void* p)
{
  ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*)p;
  schema->fields_info =field_info;
  schema->fill_table =  fill_table;
  return 0;
}

static struct st_mysql_information_schema plugin_descriptor =
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

maria_declare_plugin(thread_pool_queues)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
    & plugin_descriptor,
    "THREAD_POOL_QUEUES",
    "Vladislav Vaintroub",
    "Provides information about threadpool queues.",
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
