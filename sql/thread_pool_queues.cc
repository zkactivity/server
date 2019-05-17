#include <mysql_version.h>
#include <mysql/plugin.h>

#include <my_global.h>
#include <sql_class.h> 
#include <table.h>
#include <mysql/plugin.h>
#include <sql_show.h>
#include <threadpool_generic.h>

static ST_FIELD_INFO queues_fields_info[] =
{
  {"GROUP_ID", 6, MYSQL_TYPE_LONGLONG, 0, 0, "GROUP_ID", 0},
  {"ORDINAL_POSITION",6,MYSQL_TYPE_LONGLONG, 0, 0, "ORDINAL_POSITION", 0},
  {"PRIORITY", 19, MYSQL_TYPE_LONGLONG, 0, 0, "PRIORITY", 0},
  {"CONNECTION_ID", 19, MYSQL_TYPE_LONGLONG, 0, 0, "CONNECTION_ID", 0},
  {"QUEUEING_TIME_MICROSECONDS", 19, MYSQL_TYPE_LONGLONG, 0,0, "QUEUEING_TIME_MICROSECONDS"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};

typedef connection_queue_t::List::Iterator connection_t_iterator;
static int fill_queues_info(THD* thd, TABLE_LIST* tables, COND*)
{
  if (!all_groups)
    return 0;

  TABLE* table = tables->table;

  for (uint group_id = 0; 
       group_id < threadpool_max_size && all_groups[group_od].pollfd != INVALID_HANDLE_VALUE; 
       group_id++)
  {
    thread_group_t* group = &all_groups[group_id];

    mysql_mutex_lock(group->mutex);
    bool err=false;
    for(uint prio=0; prio < NQUEUES && !err; prio++)
    {
      connection_queue_t *queue = &group->queues[prio];
      for(;;)
      { 
         /* .. */
         table->field[0]->store(group, group_id);
         /*....*/
         err = schema_table_store_record(thd, table);
         if (err)
           break;
      }
    }
    mysql_mutex_unlock(group->mutex);
    if(err)
      return 1;
  }
  return 0;
}

static int tp_groups_info_init(void* p)
{
  ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*)p;
  schema->fields_info = queues_fields_info;
  schema->fill_table = fill_queues_info;
  return 0;
}

static struct st_mysql_information_schema plugin_descriptor =
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

maria_declare_plugin(thread_pool_groups)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
    & plugin_descriptor,
    "THREAD_POOL_QUEUES",
    "Vladislav Vaintroub",
    "Provides information about threadpool queues.",
    PLUGIN_LICENSE_GPL,
    tp_groups_info_init,
    0,
    0x0100,
    NULL,
    NULL,
    "1.0",
    MariaDB_PLUGIN_MATURITY_STABLE
}
maria_declare_plugin_end;