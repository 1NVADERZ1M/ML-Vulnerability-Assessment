/* Copyright (C) 2004 Michel Arboi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "exec.h"
#include "nasl.h"
#include "nasl_debug.h"
#include "nasl_func.h"
#include "nasl_global_ctxt.h"
#include "nasl_init.h"
#include "nasl_lex_ctxt.h"
#include "nasl_tree.h"
#include "nasl_var.h"

#include <string.h>

#undef G_LOG_DOMAIN
/**
 * @brief GLib logging domain.
 */
#define G_LOG_DOMAIN "lib  nasl"

/**
 * @brief Define struct to store information about a called function.
 */
typedef struct st_func_info
{
  gchar *func_name;   /**< Function name */
  gchar *caller_func; /**< Name of the function from where it is called */
  gchar *caller_file; /**< Name of the file from where it is called */
} func_info;

char *nasl_name;

/**
 * @brief Free a func_info structure.
 *
 * @param[in] defined_var List with all defined variables
 *
 */
void
free_list_func (func_info *data)
{
  g_free (data->func_name);
  g_free (data->caller_func);
  g_free (data->caller_file);
  memset (data, '\0', sizeof (func_info));
}

/**
 * @brief Add keywords to the varnames list.
 *
 * @param[in,out] defined_var List with all defined variables
 *
 */
void
add_predef_varname (GSList **defined_var)
{
  int i;
  gchar *keywords[] = {"ACT_UNKNOWN",  "description",    "NULL", "SCRIPT_NAME",
                       "COMMAND_LINE", "_FCT_ANON_ARGS", NULL};

  for (i = 0; keywords[i] != NULL; i++)
    *defined_var = g_slist_prepend (*defined_var, keywords[i]);
  add_nasl_library (defined_var);
}

/**
 * @brief This function is called by g_slist_find_custom.
 *
 * @param[in] lelem Element of GSList.
 * @param[in] data func_info structure to be found.
 *
 * @return 0 on success, non 0 otherwise.
 */
gint
list_cmp1 (gconstpointer lelem, gconstpointer data)
{
  if (data)
    {
      gchar *lala = g_strdup (((func_info *) lelem)->func_name);
      return (g_strcmp0 (lala, data));
    }
  return -1;
}

/**
 * @brief Check if an undefined called function is needed or not.
 *        This is the case in which the function is called from a
 *        neested and defined function but never called.
 * @return 1 if the function is needed, 0 otherwise.
 */
gint
reverse_search (GSList **def_func_tree, GSList *finfo)
{
  func_info *fdata = finfo->data;
  GSList *finfo_aux;

  // The file name is the original file to be tested. It is not an include.
  if (!g_strcmp0 (fdata->caller_file, nasl_name)
      && !g_str_has_suffix (nasl_name, ".inc"))
    return 1;

  // The function is it self.
  if (!g_strcmp0 (fdata->func_name, fdata->caller_func))
    return 0;

  // I go up in the tree of called and defined functions.
  if ((finfo_aux = g_slist_find_custom (*def_func_tree, fdata->caller_func,
                                        (GCompareFunc) list_cmp1))
      != NULL)
    if (reverse_search (def_func_tree, finfo_aux))
      return 1;

  return 0;
}

/**
 * @brief This function is called by g_slist_find_custom.
 *
 * @param[in] lelem Element of GSList.
 * @param[in] data str to be found in the list.
 *
 * @return 0 on success, non 0 otherwise.
 */
gint
list_cmp (gconstpointer lelem, gconstpointer data)
{
  if (data)
    return (g_strcmp0 (lelem, data));

  return -1;
}

/**
 * @brief This function is called by g_hash_table_foreach to check if
 *        an include file was used or not. If the file is not used, it is added
 *        to a list.
 *
 * @param[in] key Element key of GHashTable.
 * @param[in] value Element value for a key of GHashTable.
 * @param[in] unusedfiles List with unused .inc files.
 *
 */
void
check_called_files (gpointer key, gpointer value, GSList **unusedfiles)
{
  if (key != NULL)
    if (!g_strcmp0 (value, "NO"))
      *unusedfiles = g_slist_prepend (*unusedfiles, key);
}

/**
 * @brief It shows a msg for unused included files.
 *
 * @param[in] filename Filename of the not used inc file.
 * @param[in] lexic nasl context.
 *
 */
void
print_uncall_files (gpointer filename, gpointer lexic)
{
  if (filename != NULL)
    {
      nasl_perror (lexic, "The included file '%s' is never used.",
                   (char *) filename);
      lexic = NULL;
    }
}

/**
 * @brief Loads all defined functions. Also, It constructs a tree of called
 *        functions to help recognize a not defined function which is never
 *        called (nested functions).
 */
tree_cell *
nasl_lint_def (lex_ctxt *lexic, tree_cell *st, int lint_mode,
               GHashTable **include_files, GHashTable **func_fnames_tab,
               gchar *err_fname, GSList **called_funcs, GSList **def_func_tree)
{
  int i;
  tree_cell *ret = FAKE_CELL;
  char *incname = NULL;
  gchar *tmp_filename = NULL;
  nasl_func *pf;
  static gchar *current_fun_def = NULL;

  if (st->type == NODE_FUN_CALL)
    {
      pf = get_func_ref_by_name (lexic, st->x.str_val);
      if (pf == NULL)
        {
          g_hash_table_insert (*func_fnames_tab, g_strdup (st->x.str_val),
                               g_strdup (err_fname));
        }

      /* Save in a list the name of the called function, the file where it
         is called from, and the function where it is called from. This will
         help to know if a called function is really needed, or it was just
         called by another defined function which is never called. */
      func_info *finfo = g_malloc0 (sizeof (func_info));
      finfo->func_name = g_strdup (st->x.str_val);
      finfo->caller_file = g_strdup (err_fname ? err_fname : nasl_name);
      finfo->caller_func = g_strdup (current_fun_def);
      *def_func_tree = g_slist_prepend (*def_func_tree, finfo);

      /* Check if function parameters are used multiple times. Only check
       * this if we are in lint mode 1 to not check it multiple times. */
      if (lint_mode == 1)
        {
          GSList *func_params = NULL;
          int linenum = st->line_nb;
          tree_cell *args = st->link[0];
          for (; args != NULL; args = args->link[1])
            {
              if (args->x.str_val)
                {
                  /* Check if param was already used */
                  if (!g_slist_find_custom (func_params, args->x.str_val,
                                            (GCompareFunc) list_cmp))
                    func_params =
                      g_slist_prepend (func_params, args->x.str_val);
                  else
                    {
                      g_message ("%s: Error at or near line %d. "
                                 "Parameter \"%s\" passed to function \"%s\" "
                                 "was provided multiple times.",
                                 finfo->caller_file, linenum, args->x.str_val,
                                 finfo->func_name);
                      g_slist_free (func_params);
                      return NULL;
                    }
                }
            }
          g_slist_free (func_params);
        }
    }

  switch (st->type)
    {
    case NODE_FUN_DEF:
      /* with lint_mode = 0 check if this function was declared twice*/
      if (lint_mode == 0)
        {
          if (decl_nasl_func (lexic, st, lint_mode) == NULL)
            ret = NULL;
          return ret;
        }
      /* Check if it was already added */
      if (!g_slist_find_custom (*called_funcs, st->x.str_val,
                                (GCompareFunc) list_cmp))
        {
          return FAKE_CELL;
        }

      /* x.str_val = function name, [0] = argdecl, [1] = block */
      decl_nasl_func (lexic, st, lint_mode);
      current_fun_def = g_strdup (st->x.str_val);
      incname = g_strdup (nasl_get_filename (st->x.str_val));
      g_hash_table_replace (*include_files, incname, g_strdup ("NO"));
      tmp_filename = g_strdup (nasl_get_filename (NULL));
      err_fname = g_strdup (incname);
      /* fallthrough */

    default:
      for (i = 0; i < 4; i++)
        if (st->link[i] != NULL && st->link[i] != FAKE_CELL)
          if ((ret = nasl_lint_def (lexic, st->link[i], lint_mode,
                                    include_files, func_fnames_tab, err_fname,
                                    called_funcs, def_func_tree))
              == NULL)
            return NULL;

      if (st->type == NODE_FUN_DEF)
        {
          if (tmp_filename)
            nasl_set_filename (tmp_filename);
          g_free (tmp_filename);
        }
      return ret;
    }
}

/**
 * @brief Check if a called function was defined.
 */
tree_cell *
nasl_lint_call (lex_ctxt *lexic, tree_cell *st, GHashTable **include_files,
                GHashTable **func_fnames_tab, gchar *err_fname,
                GSList **called_funcs, GSList **def_func_tree)
{
  int i;
  tree_cell *ret = FAKE_CELL;
  nasl_func *pf;
  char *incname = NULL;
  static int defined_flag = 0;

  /** This checks if a defined function is called. If it is never called
   * it does not go deeper.
   */
  if (st->type == NODE_FUN_DEF)
    {
      if (!g_slist_find_custom (*called_funcs, st->x.str_val,
                                (GCompareFunc) list_cmp))
        {
          return FAKE_CELL;
        }
    }

  switch (st->type)
    {
    case CONST_DATA:
    case CONST_STR:
      if (st->x.str_val != NULL && defined_flag == 1)
        {
          decl_nasl_func (lexic, st, 1);
          defined_flag = 0;
        }
      return FAKE_CELL;

    case NODE_FUN_CALL:
      pf = get_func_ref_by_name (lexic, st->x.str_val);
      if (pf == NULL)
        {
          incname = g_hash_table_lookup (*func_fnames_tab, st->x.str_val);
          incname ? nasl_set_filename (incname) : "unknown";
          lexic->line_nb = st->line_nb;

          GSList *called_f_aux;
          called_f_aux = g_slist_find_custom (*def_func_tree, st->x.str_val,
                                              (GCompareFunc) list_cmp1);
          if (called_f_aux != NULL)
            if (reverse_search (def_func_tree, called_f_aux))
              {
                nasl_perror (lexic, "Undefined function '%s'\n", st->x.str_val);
                return NULL;
              }
        }
      if (*include_files && st->x.str_val)
        {
          if (g_hash_table_lookup (*include_files,
                                   nasl_get_filename (st->x.str_val)))
            {
              incname = g_strdup (nasl_get_filename (st->x.str_val));
              g_hash_table_replace (*include_files, incname, g_strdup ("YES"));
            }
        }
      if (g_strcmp0 (st->x.str_val, "defined_func") == 0)
        defined_flag = 1;
      /* fallthrough */

    default:
      for (i = 0; i < 4; i++)
        if (st->link[i] != NULL && st->link[i] != FAKE_CELL)
          if ((ret = nasl_lint_call (lexic, st->link[i], include_files,
                                     func_fnames_tab, err_fname, called_funcs,
                                     def_func_tree))
              == NULL)
            return NULL;
      return ret;
    }
}

/**
 * @brief Consider all cases in which a variable is set, and add it to a list.
 *        If a variable is read, it checks if it was previously added to the
 *        list.
 */
tree_cell *
nasl_lint_defvar (lex_ctxt *lexic, tree_cell *st, GHashTable **include_files,
                  GHashTable **func_fnames_tab, gchar *err_fname,
                  GSList **defined_var, GSList **called_funcs)
{
  int i;
  tree_cell *ret = FAKE_CELL;
  static int defined_fn_mode = 0;
  static int defined_var_mode = 0;
  static int def_glob_var = 0;
  static GSList *local_var_list = NULL;

  /** This checks if a defined function is called. If it is never called
   * it does not go deeper.
   */
  if (st->type == NODE_FUN_DEF)
    {
      if (!g_slist_find_custom (*called_funcs, st->x.str_val,
                                (GCompareFunc) list_cmp))
        {
          return FAKE_CELL;
        }
    }

  if ((defined_fn_mode == 1 || def_glob_var) && st->type != NODE_DECL)
    {
      defined_fn_mode = 0;
      def_glob_var = 0;
    }

  /* A variable will be defined, then set the mode variable. */
  if ((st->type == NODE_AFF || st->type == EXPR_NOT || st->type == EXPR_INCR
       || st->type == NODE_PLUS_EQ)
      && defined_var_mode == 0)
    defined_var_mode = 1;
  else if ((st->type == NODE_FUN_DEF || st->type == NODE_LOCAL)
           && defined_fn_mode == 0)
    defined_fn_mode = 1;

  else if (st->type == NODE_GLOBAL)
    def_glob_var = 1;

  /* The variable is being defined. Therefore is save into the
   * global list only if was not previously added in local list.
   */
  else if ((st->type == NODE_VAR || st->type == NODE_ARRAY_EL)
           && (defined_var_mode == 1 || defined_fn_mode == 1))
    {
      if (st->x.str_val != NULL)
        {
          if (!g_slist_find_custom (local_var_list, st->x.str_val,
                                    (GCompareFunc) list_cmp))
            *defined_var = g_slist_prepend (*defined_var, st->x.str_val);
          defined_var_mode = 0;
        }
    }
  /** It is a local variable and it is added in special list,
   *  which will be cleaned at the end of the function.
   */
  else if (st->type == NODE_DECL && st->x.str_val != NULL)
    {
      if (defined_fn_mode == 1)
        local_var_list = g_slist_prepend (local_var_list, st->x.str_val);
      if (def_glob_var == 1)
        *defined_var = g_slist_prepend (*defined_var, st->x.str_val);
    }
  /* Special case foreach. */
  else if (st->type == NODE_FOREACH)
    {
      if (st->x.str_val != NULL)
        *defined_var = g_slist_prepend (*defined_var, st->x.str_val);
    }
  // The variable is used. It checks if the variable was defined
  else if (st->type == NODE_VAR && defined_var_mode == 0)
    {
      if (!g_slist_find_custom (*defined_var, st->x.str_val,
                                (GCompareFunc) list_cmp)
          && !g_slist_find_custom (local_var_list, st->x.str_val,
                                   (GCompareFunc) list_cmp))
        {
          lexic->line_nb = st->line_nb;
          nasl_perror (lexic, "The variable %s was not declared",
                       st->x.str_val);
          return NULL;
        }
    }

  for (i = 0; i < 4; i++)
    if (st->link[i] != NULL && st->link[i] != FAKE_CELL)
      if ((ret = nasl_lint_defvar (lexic, st->link[i], include_files,
                                   func_fnames_tab, err_fname, defined_var,
                                   called_funcs))
          == NULL)
        return NULL;

  /** Leaving the function definition, the local variables list
   *  is cleaned.
   */
  if (st->type == NODE_FUN_DEF)
    {
      g_slist_free (local_var_list);
      local_var_list = NULL;
    }

  return ret;
}
/**
 * @brief Make a list of all called functions.
 */
tree_cell *
make_call_func_list (lex_ctxt *lexic, tree_cell *st, GSList **called_funcs)
{
  int i;
  tree_cell *ret = FAKE_CELL;
  nasl_func *pf = NULL;

  switch (st->type)
    {
    case NODE_FUN_CALL:
      pf = get_func_ref_by_name (lexic, st->x.str_val);
      if (st->x.str_val && !pf)
        {
          *called_funcs =
            g_slist_prepend (*called_funcs, g_strdup (st->x.str_val));
        }
      /* fallthrough */

    default:
      for (i = 0; i < 4; i++)
        if (st->link[i] != NULL && st->link[i] != FAKE_CELL)
          if ((ret = make_call_func_list (lexic, st->link[i], called_funcs))
              == NULL)
            return NULL;
      return ret;
    }
}

/**
 * @brief Search for errors in a nasl script
 *
 * @param[in] lexic nasl context.
 * @param[in] st structure tree of a nasl script.
 *
 * @return FAKE_CELL if no error was found, NULL otherwise.
 */
tree_cell *
nasl_lint (lex_ctxt *lexic, tree_cell *st)
{
  lex_ctxt *lexic_aux;
  tree_cell *ret = FAKE_CELL;
  int lint_mode = 1;
  GHashTable *include_files = NULL;
  GHashTable *func_fnames_tab = NULL;
  GSList *unusedfiles = NULL;
  GSList *called_funcs = NULL;
  GSList *def_func_tree = NULL;
  gchar *err_fname = NULL;

  nasl_name = g_strdup (nasl_get_filename (st->x.str_val));
  include_files =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  func_fnames_tab =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  lexic_aux = init_empty_lex_ctxt ();
  lexic_aux->script_infos = lexic->script_infos;
  lexic_aux->oid = lexic->oid;

  /* Make a list of all called functions */
  make_call_func_list (lexic_aux, st, &called_funcs);

  /* Loads all defined functions. */
  if ((ret = nasl_lint_def (lexic_aux, st, lint_mode, &include_files,
                            &func_fnames_tab, err_fname, &called_funcs,
                            &def_func_tree))
      == NULL)
    goto fail;
  /* Check if a called function was defined. */
  if ((ret = nasl_lint_call (lexic_aux, st, &include_files, &func_fnames_tab,
                             err_fname, &called_funcs, &def_func_tree))
      == NULL)
    goto fail;

  /* Check if the included files are used or not. */
  g_hash_table_foreach (include_files, (GHFunc) check_called_files,
                        &unusedfiles);
  if (unusedfiles != NULL)
    g_slist_foreach (unusedfiles, (GFunc) print_uncall_files, lexic_aux);
  if ((g_slist_length (unusedfiles)) > 0)
    {
      ret = NULL;
      goto fail;
    }

  /* Now check that each function was loaded just once. */
  lint_mode = 0;
  if ((ret =
         nasl_lint_def (lexic, st, lint_mode, &include_files, &func_fnames_tab,
                        err_fname, &called_funcs, &def_func_tree))
      == NULL)
    goto fail;

  /* Check if a variable was declared. */
  GSList *defined_var = NULL;
  add_predef_varname (&defined_var);
  ret = nasl_lint_defvar (lexic_aux, st, &include_files, &func_fnames_tab,
                          err_fname, &defined_var, &called_funcs);
  g_slist_free (defined_var);
  defined_var = NULL;

fail:
  g_slist_free (called_funcs);
  called_funcs = NULL;
  g_slist_free_full (def_func_tree, (GDestroyNotify) free_list_func);
  def_func_tree = NULL;
  g_hash_table_destroy (include_files);
  include_files = NULL;
  g_hash_table_destroy (func_fnames_tab);
  func_fnames_tab = NULL;
  g_free (err_fname);
  g_slist_free (unusedfiles);
  unusedfiles = NULL;
  free_lex_ctxt (lexic_aux);

  return ret;
}
