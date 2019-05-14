
#include "mariadb.h"
#include "sql_base.h"


bool TABLE::set_unique_key_as_func_dep(KEY *unique_key)
{
  uint uk_parts_in_gb= 0;
  for (uint i= 0; i < unique_key->user_defined_key_parts; ++i)
  {
    if (unique_key->key_part[i].field->excl_func_dep_on_grouping_fields())
      uk_parts_in_gb++;
  }
  if (uk_parts_in_gb == unique_key->user_defined_key_parts)
  {
    // add all table fields
    bitmap_set_all(&tmp_set);
    return true;
  }
  return false;
}


void get_unique_keys_fields_for_group_by(List<TABLE_LIST> *join_list)
{
  List_iterator<TABLE_LIST> it(*join_list);
  TABLE_LIST *tbl;
  while ((tbl= it++))
  {
    if (bitmap_is_set_all(&tbl->table->tmp_set))
      continue;
    //there is primary key
    bool has_pk= false;
    if (tbl->table->s->primary_key < MAX_KEY)
    {
      KEY *pk= &tbl->table->key_info[tbl->table->s->primary_key];
      if (tbl->table->set_unique_key_as_func_dep(pk))
        has_pk= true;
    }
    if (!has_pk)
    {
      KEY *end= tbl->table->key_info + tbl->table->s->keys;
      for (KEY *k= tbl->table->key_info; k < end; k++)
        if ((k->flags & HA_NOSAME) && 
            tbl->table->set_unique_key_as_func_dep(k))
          break;
    }
  }
}


bool st_select_lex::is_select_list_determined(THD *thd,
                                              List<TABLE_LIST> *join_list,
                                              List<Item> &items_to_check)
{
  Item *item;
  List_iterator<Item> li(item_list);
  List_iterator<TABLE_LIST> it(*join_list);
  TABLE_LIST *tbl;
  while ((tbl= it++))
  {
    bitmap_clear_all(&tbl->table->tmp_set);
  }
  for (ORDER *ord= group_list.first; ord; ord= ord->next)
  {
    Item *ord_item= *ord->item;
    if (ord_item->type() == Item::FIELD_ITEM)
    {
      bitmap_set_bit(&((Item_field *)ord_item)->field->table->tmp_set,
                     ((Item_field *)ord_item)->field->field_index);
    }
  }
  // check primary and unique
  // if so -> add all fields of table
  get_unique_keys_fields_for_group_by(join_list);

  while ((item=li++))
  {
    if (!item->excl_func_dep_on_grouping_fields() &&
        !(item->real_item()->type() == Item::FIELD_ITEM &&
          item->used_tables() & OUTER_REF_TABLE_BIT))
    {
      if (item->type() == Item::FIELD_ITEM)
      {
        if (items_to_check.push_back(item, thd->mem_root))
          return true;
      }
      else
      {
        ORDER *ord;
        for (ord= group_list.first; ord; ord= ord->next)
          if ((*ord->item)->eq(item, 0))
            break;
        if (!ord && items_to_check.push_back(item, thd->mem_root))
          return true;
          
      }
    }
  }
  return false;
}


static bool collect_cond_equalities(THD *thd, Item *cond,
                                    List<Item_func_eq> &cond_equalities)
{
  if (cond && cond->type() == Item::COND_ITEM &&
      ((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
    {
      if (item->type() == Item::FUNC_ITEM &&
          ((Item_func*) item)->functype() == Item_func::EQ_FUNC &&
          cond_equalities.push_back((Item_func_eq *)item, thd->mem_root))
        return true;
    }
  }
  else if (cond->type() == Item::FUNC_ITEM &&
           ((Item_func*) cond)->functype() == Item_func::EQ_FUNC &&
           cond_equalities.push_back((Item_func_eq *)cond, thd->mem_root))
    return true;
  return false;
}


static
void extract_dependencies_from_conds(List<Item_func_eq> *cond_equalities)
{
  List_iterator<Item_func_eq> li(*cond_equalities);
  Item_func_eq *eq_item;
  Field *field;
  bool new_fields_extracted= true;

  while (new_fields_extracted && cond_equalities->elements > 0)
  {
    new_fields_extracted= false;
    while ((eq_item=li++))
    {
      for (uint i=0; i < 2; i++)
      {
        if (!eq_item->arguments()[i]->excl_func_dep_on_grouping_fields())
          continue;
        if (eq_item->arguments()[i]->type_handler_for_comparison() ==
            eq_item->compare_type_handler())
        {
          for (uint j=0; j < 2; j++)
          {
            if (i == j)
              continue;
            // case when a = concat(b,c)
            // a in GROUP BY, only concat(b,c) depends on a, not b and c
            // so we add to bitmap only if there is one field in the right func
            // situation SELECT CONCAT(b,c) ... WHERE CONCAT(b,c)=group_by_field
            field= NULL;
            if (eq_item->arguments()[j]->extract_field(&field))
            {
              bitmap_set_bit(&field->table->tmp_set, field->field_index);
              new_fields_extracted= true;
            }
          }
        }
        li.remove();
        break;
      }
    }
    li.rewind();
  }
}


bool check_functional_dependencies(THD *thd, List<TABLE_LIST> *join_list,
                                   List<Item> *items_to_check, Item *cond)
{
  if (items_to_check->elements == 0)
    return false;
  if (!cond)
  {
    my_error(ER_NO_FUNCTIONAL_DEPENDENCE_ON_GROUP_BY, MYF(0),
             items_to_check->head()->full_name());
    return true;
  }

  List<Item_func_eq> cond_equalities;
  if (collect_cond_equalities(thd, cond, cond_equalities))
    return true;
  if (!cond_equalities.elements)
  {
    //error because we already checked grouping list and select
    //list and no new equalities can be added
    my_error(ER_NO_FUNCTIONAL_DEPENDENCE_ON_GROUP_BY, MYF(0),
             items_to_check->head()->full_name());
    return true;
  }
  extract_dependencies_from_conds(&cond_equalities);
  //double check pk and unique
  get_unique_keys_fields_for_group_by(join_list);

  Item *item;
  List_iterator<Item> li(*items_to_check);
  while ((item=li++))
  {
    if (!item->excl_func_dep_on_grouping_fields())
    {
      my_error(ER_NO_FUNCTIONAL_DEPENDENCE_ON_GROUP_BY, MYF(0),
               item->full_name());
      return true;
    }
  }  
  return false;
}


